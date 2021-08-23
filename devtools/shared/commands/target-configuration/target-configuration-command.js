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
    return this._commands.targetCommand.hasTargetWatcherSupport();
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
    // If we don't have target watcher support, we can't get this value, so just
    // fall back to true. Only content tab targets can update javascriptEnabled
    // and all should have watcher support.
    if (!this._hasTargetWatcherSupport()) {
      return true;
    }

    const front = await this.getFront();
    return front.isJavascriptEnabled();
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
