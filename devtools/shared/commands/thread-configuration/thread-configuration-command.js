/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The ThreadConfigurationCommand should be used to maintain thread settings
 * sent from the client for the thread actor.
 *
 * See the ThreadConfigurationActor for a list of supported configuration options.
 */
class ThreadConfigurationCommand {
  constructor({ commands, watcherFront }) {
    this._commands = commands;
    this._watcherFront = watcherFront;
  }

  /**
   * Return a promise that resolves to the related thread configuration actor's front.
   *
   * @return {Promise<ThreadConfigurationFront>}
   */
  async getThreadConfigurationFront() {
    const front = await this._watcherFront.getThreadConfigurationActor();
    return front;
  }

  async updateConfiguration(configuration) {
    if (this._commands.targetCommand.hasTargetWatcherSupport()) {
      const threadConfigurationFront = await this.getThreadConfigurationFront();
      const updatedConfiguration = await threadConfigurationFront.updateConfiguration(
        configuration
      );
      this._configuration = updatedConfiguration;
    } else {
      const threadFronts = await this._commands.targetCommand.getAllFronts(
        this._commands.targetCommand.ALL_TYPES,
        "thread"
      );
      await Promise.all(
        threadFronts.map(threadFront =>
          threadFront.pauseOnExceptions(
            configuration.pauseOnExceptions,
            configuration.ignoreCaughtExceptions
          )
        )
      );
    }
  }
}

module.exports = ThreadConfigurationCommand;
