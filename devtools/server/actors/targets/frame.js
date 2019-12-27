/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a frame / docShell in the content process (where the actual
 * content lives).
 *
 * This actor extends BrowsingContextTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

var { Cr } = require("chrome");
var {
  BrowsingContextTargetActor,
  browsingContextTargetPrototype,
} = require("devtools/server/actors/targets/browsing-context");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { frameTargetSpec } = require("devtools/shared/specs/targets/frame");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of BrowsingContextTargetActor.prototype
 */
const frameTargetPrototype = extend({}, browsingContextTargetPrototype);

/**
 * Target actor for a frame / docShell in the content process.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param docShell
 *        The |docShell| for the debugged frame.
 */
frameTargetPrototype.initialize = function(connection, docShell) {
  BrowsingContextTargetActor.prototype.initialize.call(this, connection);

  // Retrieve the message manager from the provided docShell.
  // Note that messageManager also has a docShell property, but in some
  // situations docShell.messageManager.docShell points to a different docShell.
  this._messageManager = docShell.messageManager;

  this.traits.reconfigure = false;
  this._sendForm = this._sendForm.bind(this);
  this._messageManager.addMessageListener("debug:form", this._sendForm);

  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true,
  });
};

Object.defineProperty(frameTargetPrototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true,
});

frameTargetPrototype.exit = function() {
  if (this._sendForm) {
    try {
      this._messageManager.removeMessageListener("debug:form", this._sendForm);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NULL_POINTER) {
        throw e;
      }
      // In some cases, especially when using messageManagers in non-e10s mode, we reach
      // this point with a dead messageManager which only throws errors but does not
      // seem to indicate in any other way that it is dead.
    }
    this._sendForm = null;
  }

  BrowsingContextTargetActor.prototype.exit.call(this);

  this._messageManager = null;
};

/**
 * On navigation events, our URL and/or title may change, so we update our
 * counterpart in the parent process that participates in the tab list.
 */
frameTargetPrototype._sendForm = function() {
  this._messageManager.sendAsyncMessage("debug:form", this.form());
};

exports.FrameTargetActor = ActorClassWithSpec(
  frameTargetSpec,
  frameTargetPrototype
);
