/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Cr } = require("chrome");
var { TabActor, tabPrototype } = require("devtools/server/actors/tab");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { tabSpec } = require("devtools/shared/specs/tab");

/**
 * Tab actor for documents living in a child process.
 *
 * Depends on TabActor, defined in tab.js.
 */

/**
 * Creates a tab actor for handling requests to the single tab, like
 * attaching and detaching. ContentActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param chromeGlobal
 *        The content script global holding |content| and |docShell| properties for a tab.
 * @param prefix
 *        the prefix used in protocol to create IDs for each actor.
 *        Used as ID identifying this particular TabActor from the parent process.
 */

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of TabActor.prototype
 * */

const contentPrototype = extend({}, tabPrototype);

contentPrototype.initialize = function(connection, chromeGlobal) {
  this._chromeGlobal = chromeGlobal;
  TabActor.prototype.initialize.call(this, connection, chromeGlobal);
  this.traits.reconfigure = false;
  this._sendForm = this._sendForm.bind(this);
  this._chromeGlobal.addMessageListener("debug:form", this._sendForm);

  Object.defineProperty(this, "docShell", {
    value: this._chromeGlobal.docShell,
    configurable: true
  });
};

Object.defineProperty(contentPrototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true
});

contentPrototype.exit = function() {
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

  TabActor.prototype.exit.call(this);

  this._chromeGlobal = null;
};

/**
 * On navigation events, our URL and/or title may change, so we update our
 * counterpart in the parent process that participates in the tab list.
 */
contentPrototype._sendForm = function() {
  this._chromeGlobal.sendAsyncMessage("debug:form", this.form());
};

exports.ContentActor = ActorClassWithSpec(tabSpec, contentPrototype);
