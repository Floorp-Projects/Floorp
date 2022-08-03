/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cr } = require("chrome");
const flags = require("devtools/shared/flags");

/**
 * A transport for the debugging protocol that uses nsIMessageManagers to
 * exchange packets with servers running in child processes.
 *
 * In the parent process, |mm| should be the nsIMessageSender for the
 * child process. In a child process, |mm| should be the child process
 * message manager, which sends packets to the parent.
 *
 * |prefix| is a string included in the message names, to distinguish
 * multiple servers running in the same child process.
 *
 * This transport exchanges messages named 'debug:<prefix>:packet', where
 * <prefix> is |prefix|, whose data is the protocol packet.
 */
function ChildDebuggerTransport(mm, prefix) {
  this._mm = mm;
  this._messageName = "debug:" + prefix + ":packet";
}

/*
 * To avoid confusion, we use 'message' to mean something that
 * nsIMessageSender conveys, and 'packet' to mean a remote debugging
 * protocol packet.
 */
ChildDebuggerTransport.prototype = {
  constructor: ChildDebuggerTransport,

  hooks: null,

  _addListener() {
    this._mm.addMessageListener(this._messageName, this);
  },

  _removeListener() {
    try {
      this._mm.removeMessageListener(this._messageName, this);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NULL_POINTER) {
        throw e;
      }
      // In some cases, especially when using messageManagers in non-e10s mode, we reach
      // this point with a dead messageManager which only throws errors but does not
      // seem to indicate in any other way that it is dead.
    }
  },

  ready() {
    this._addListener();
  },

  close(options) {
    this._removeListener();
    if (this.hooks.onTransportClosed) {
      this.hooks.onTransportClosed(null, options);
    }
  },

  receiveMessage({ data }) {
    this.hooks.onPacket(data);
  },

  /**
   * Helper method to ensure a given `object` can be sent across message manager
   * without being serialized to JSON.
   * See https://searchfox.org/mozilla-central/rev/6bfadf95b4a6aaa8bb3b2a166d6c3545983e179a/dom/base/nsFrameMessageManager.cpp#458-469
   */
  _canBeSerialized(object) {
    try {
      const holder = new StructuredCloneHolder(object);
      holder.deserialize(this);
    } catch (e) {
      return false;
    }
    return true;
  },

  pathToUnserializable(object) {
    for (const key in object) {
      const value = object[key];
      if (!this._canBeSerialized(value)) {
        if (typeof value == "object") {
          return [key].concat(this.pathToUnserializable(value));
        }
        return [key];
      }
    }
    return [];
  },

  send(packet) {
    if (flags.testing && !this._canBeSerialized(packet)) {
      const attributes = this.pathToUnserializable(packet);
      let msg =
        "Following packet can't be serialized: " + JSON.stringify(packet);
      msg += "\nBecause of attributes: " + attributes.join(", ") + "\n";
      msg += "Did you pass a function or an XPCOM object in it?";
      throw new Error(msg);
    }
    try {
      this._mm.sendAsyncMessage(this._messageName, packet);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NULL_POINTER) {
        throw e;
      }
      // In some cases, especially when using messageManagers in non-e10s mode, we reach
      // this point with a dead messageManager which only throws errors but does not
      // seem to indicate in any other way that it is dead.
    }
  },

  startBulkSend() {
    throw new Error("Can't send bulk data to child processes.");
  },
};

exports.ChildDebuggerTransport = ChildDebuggerTransport;
