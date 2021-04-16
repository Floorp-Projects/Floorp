/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The TargetConfigurationCommand should be used to populate the DevTools server
 * with settings read from the client side but which impact the server.
 * For instance, "disable cache" is a feature toggled via DevTools UI (client),
 * but which should be communicated to the targets (server).
 *
 * See the TargetConfigurationActor for a list of supported configuration options.
 */
class TargetConfigurationCommand {
  constructor({ commands, watcherFront }) {
    this._commands = commands;
    this._watcherFront = watcherFront;
  }

  /**
   * Return a promise that resolves to the related target configuration actor's front.
   *
   * @return {Promise<TargetConfigurationFront>}
   */
  async getFront() {
    const front = await this._watcherFront.getTargetConfigurationActor();

    if (!this._configuration) {
      // Retrieve initial data from the front
      this._configuration = front.initialConfiguration;
    }

    return front;
  }

  _hasTargetWatcherSupport() {
    return this._commands.targetCommand.hasTargetWatcherSupport(
      "target-configuration"
    );
  }

  /**
   * Retrieve the current map of configuration options pushed to the server.
   */
  get configuration() {
    return this._configuration || {};
  }

  async updateConfiguration(configuration) {
    if (this._hasTargetWatcherSupport()) {
      const front = await this.getFront();
      const updatedConfiguration = await front.updateConfiguration(
        configuration
      );
      // Update the client-side copy of the DevTools configuration
      this._configuration = updatedConfiguration;
    } else {
      await this._commands.targetCommand.targetFront.reconfigure({
        options: configuration,
      });
    }
  }

  async isJavascriptEnabled() {
    if (
      this._hasTargetWatcherSupport() &&
      // `javascriptEnabled` is first read by the target and then forwarded by
      // the toolbox to the TargetConfigurationCommand, so it might be undefined at this
      // point.
      typeof this.configuration.javascriptEnabled !== "undefined"
    ) {
      return this.configuration.javascriptEnabled;
    }

    // If the TargetConfigurationActor does not know the value yet, or if the target don't
    // support the Watcher + configuration actor, fallback on the initial value cached by
    // the target front.
    return this._commands.targetCommand.targetFront._javascriptEnabled;
  }

  /**
   * Enable or disable touch events simulation
   *
   * @param {String|null} flag: The value to set for the touchEventsOverride flag.
   *                      Pass null to reset the flag to its original value.
   * @returns {Boolean} Returns true if the page needs to be reloaded (so the page can
   *                    acknowledge the new state).
   */
  async setTouchEventsOverride(flag) {
    // We need to set the flag on the parent process
    await this.updateConfiguration({
      touchEventsOverride: flag,
    });

    // And start the touch simulation within the content process.
    // Note that this only handle current top-level document. When Fission is enabled, this
    // doesn't enable touch simulation in remote iframes (See Bug 1704028).
    // This also does not handle further navigation to a different origin (aka target switch),
    // which should be fixed in Bug 1704029.
    const responsiveFront = await this._commands.targetCommand.targetFront.getFront(
      "responsive"
    );
    const reloadNeeded = await responsiveFront.toggleTouchSimulator({
      enable: flag === "enabled",
    });

    return reloadNeeded;
  }

  /**
   * Change orientation type and angle (that can be accessed through screen.orientation in
   * the content page) and simulates the "orientationchange" event when the device screen
   * was rotated.
   * Note that this will only be effective if the Responsive Design Mode is enabled.
   *
   * @param {Object} options
   * @param {String} options.type: The orientation type of the rotated device.
   * @param {Number} options.angle: The rotated angle of the device.
   * @param {Boolean} options.isViewportRotated: Whether or not screen orientation change
   *                 is a result of rotating the viewport. If true, an "orientationchange"
   *                 event will be dispatched in the content window.
   */
  async simulateScreenOrientationChange({ type, angle, isViewportRotated }) {
    // We need to call the method on the parent process
    await this.updateConfiguration({
      rdmPaneOrientation: { type, angle },
    });

    // Don't dispatch the "orientationchange" event if orientation change is a result
    // of switching to a new device, location change, or opening RDM.
    if (!isViewportRotated) {
      return;
    }

    const responsiveFront = await this._commands.targetCommand.targetFront.getFront(
      "responsive"
    );
    await responsiveFront.dispatchOrientationChangeEvent();
  }
}

module.exports = TargetConfigurationCommand;
