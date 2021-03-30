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
}

module.exports = TargetConfigurationCommand;
