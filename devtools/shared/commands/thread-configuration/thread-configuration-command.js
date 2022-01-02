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
      // Remove thread options that are not currently supported by
      // the thread configuration actor.
      const filteredConfiguration = Object.fromEntries(
        Object.entries(configuration).filter(
          ([key, value]) => !["breakpoints", "eventBreakpoints"].includes(key)
        )
      );

      const threadConfigurationFront = await this.getThreadConfigurationFront();
      const updatedConfiguration = await threadConfigurationFront.updateConfiguration(
        filteredConfiguration
      );
      this._configuration = updatedConfiguration;
    }

    let threadFronts = await this._commands.targetCommand.getAllFronts(
      this._commands.targetCommand.ALL_TYPES,
      "thread"
    );

    // Lets always call reconfigure for all the target types that do not
    // have target watcher support yet. e.g In the browser, even
    // though `hasTargetWatcherSupport()` is true, only
    // FRAME and CONTENT PROCESS targets use watcher actors,
    // WORKER targets are supported via the legacy listerners.
    threadFronts = threadFronts.filter(
      threadFront =>
        !this._commands.targetCommand.hasTargetWatcherSupport(
          threadFront.targetFront.targetType
        )
    );

    await Promise.all(
      threadFronts.map(threadFront => threadFront.reconfigure(configuration))
    );
  }
}

module.exports = ThreadConfigurationCommand;
