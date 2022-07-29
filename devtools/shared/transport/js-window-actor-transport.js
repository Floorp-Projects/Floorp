/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * DevTools transport relying on JS Window Actors. This is an experimental
 * transport. It is only used when using the JS Window Actor based frame
 * connector. In that case this transport will be used to communicate between
 * the DevToolsServer living in the parent process and the DevToolsServer
 * living in the process of the target frame.
 *
 * This is intended to be a replacement for child-transport.js which is a
 * message-manager based transport.
 */
class JsWindowActorTransport {
  constructor(jsWindowActor, prefix) {
    this.hooks = null;
    this._jsWindowActor = jsWindowActor;
    this._prefix = prefix;

    this._onPacketReceived = this._onPacketReceived.bind(this);
  }

  _addListener() {
    this._jsWindowActor.on("packet-received", this._onPacketReceived);
  }

  _removeListener() {
    this._jsWindowActor.off("packet-received", this._onPacketReceived);
  }

  ready() {
    this._addListener();
  }

  /**
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  close(options) {
    this._removeListener();
    if (this.hooks.onTransportClosed) {
      this.hooks.onTransportClosed(null, options);
    }
  }

  _onPacketReceived(eventName, { data }) {
    const { prefix, packet } = data;
    if (prefix === this._prefix) {
      this.hooks.onPacket(packet);
    }
  }

  send(packet) {
    this._jsWindowActor.sendPacket(packet, this._prefix);
  }

  startBulkSend() {
    throw new Error("startBulkSend not implemented for JsWindowActorTransport");
  }
}

exports.JsWindowActorTransport = JsWindowActorTransport;
