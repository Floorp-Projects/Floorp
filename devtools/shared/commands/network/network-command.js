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
    const networkContentFront =
      await this.commands.targetCommand.targetFront.getFront("networkContent");
    const { channelId } = await networkContentFront.sendHTTPRequest(data);
    return { channelId };
  }

  /*
   * Get the list of blocked URL filters.
   *
   * A URL filter is a RegExp string so that one filter can match many URLs.
   * It can be an absolute URL to match only one precise request:
   *   http://mozilla.org/index.html
   * Or just a string which would match all URL containing this string:
   *   mozilla
   * Or a RegExp to match various types of URLs:
   *   http://*mozilla.org/*.css
   *
   * @return {Array}
   *         List of all currently blocked URL filters.
   */
  async getBlockedUrls() {
    const networkParentFront = await this.watcherFront.getNetworkParentActor();
    return networkParentFront.getBlockedUrls();
  }

  /**
   * Updates the list of blocked URL filters.
   *
   * @param {Array} urls
   *        An array of URL filter strings.
   *        See getBlockedUrls for definition of URL filters.
   */
  async setBlockedUrls(urls) {
    const networkParentFront = await this.watcherFront.getNetworkParentActor();
    return networkParentFront.setBlockedUrls(urls);
  }

  /**
   * Block only one additional URL filter
   *
   * @param {String} url
   *        URL filter to block.
   *        See getBlockedUrls for definition of URL filters.
   */
  async blockRequestForUrl(url) {
    const networkParentFront = await this.watcherFront.getNetworkParentActor();
    return networkParentFront.blockRequest({ url });
  }

  /**
   * Stop blocking only one specific URL filter
   *
   * @param {String} url
   *        URL filter to unblock.
   *        See getBlockedUrls for definition of URL filters.
   */
  async unblockRequestForUrl(url) {
    const networkParentFront = await this.watcherFront.getNetworkParentActor();
    return networkParentFront.unblockRequest({ url });
  }

  destroy() {}
}

module.exports = NetworkCommand;
