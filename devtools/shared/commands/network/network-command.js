/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class NetworkCommand {
  /**
   * This class helps listen, inspect and control network requests.
   *
   * @param {DescriptorFront} descriptorFront
   *        The context to inspect identified by this descriptor.
   * @param {WatcherFront} watcherFront
   *        If available, a reference to the related Watcher Front.
   * @param {Object} commands
   *        The commands object with all interfaces defined from devtools/shared/commands/
   */
  constructor({ descriptorFront, watcherFront, commands }) {
    this.commands = commands;
    this.descriptorFront = descriptorFront;
    this.watcherFront = watcherFront;
  }

  /**
   * Send a HTTP request data payload
   *
   * @param {object} data data payload would like to sent to backend
   */
  async sendHTTPRequest(data) {
    // By default use the top-level target, but we might at some point
    // allow using another target.
    const networkContentFront = await this.commands.targetCommand.targetFront.getFront(
      "networkContent"
    );
    const { channelId } = await networkContentFront.sendHTTPRequest(data);
    return { channelId };
  }

  destroy() {}
}

module.exports = NetworkCommand;
