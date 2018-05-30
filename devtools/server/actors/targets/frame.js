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
  browsingContextTargetPrototype
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
 * @param chromeGlobal
 *        The content script global holding |content| and |docShell| properties for a tab.
 * @param prefix
 *        the prefix used in protocol to create IDs for each actor.
 *        Used as ID identifying this particular target actor from the parent process.
 */
frameTargetPrototype.initialize = function(connection, chromeGlobal) {
  this._chromeGlobal = chromeGlobal;
  BrowsingContextTargetActor.prototype.initialize.call(this, connection, chromeGlobal);
  this.traits.reconfigure = false;
  this._sendForm = this._sendForm.bind(this);
  this._chromeGlobal.addMessageListener("debug:form", this._sendForm);

  Object.defineProperty(this, "docShell", {
    value: this._chromeGlobal.docShell,
    configurable: true
  });
};

Object.defineProperty(frameTargetPrototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true
});

frameTargetPrototype.exit = function() {
  if (this._sendForm) {
    try {
      this._chromeGlobal.removeMessageListener("debug:form", this._sendForm);
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

  this._chromeGlobal = null;
};

/**
 * On navigation events, our URL and/or title may change, so we update our
 * counterpart in the parent process that participates in the tab list.
 */
frameTargetPrototype._sendForm = function() {
  this._chromeGlobal.sendAsyncMessage("debug:form", this.form());
};

exports.FrameTargetActor = ActorClassWithSpec(frameTargetSpec, frameTargetPrototype);
