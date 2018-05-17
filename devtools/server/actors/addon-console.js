/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ConsoleAPIListener } = require("devtools/server/actors/webconsole/listeners");
var { update } = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(this, "WebConsoleActor", "devtools/server/actors/webconsole", true);

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { webconsoleSpec } = require("devtools/shared/specs/webconsole");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of TabActor.prototype
 * */
const addonConsolePrototype = extend({}, WebConsoleActor.prototype);

/**
 * The AddonConsoleActor implements capabilities needed for the add-on web
 * console feature.
 *
 * @constructor
 * @param object addon
 *        The add-on that this console watches.
 * @param object connection
 *        The connection to the client, DebuggerServerConnection.
 * @param object parentActor
 *        The parent BrowserAddonActor actor.
 */
addonConsolePrototype.initialize = function(addon, connection, parentActor) {
  this.addon = addon;
  Actor.prototype.initialize.call(this, connection);
  WebConsoleActor.call(this, connection, parentActor);
};

update(addonConsolePrototype, {
  // TODO: remove once webconsole is updated to protocol.js, Bug #1450946
  actorPrefix: "addonConsole",

  /**
   * The add-on that this console watches.
   */
  addon: null,

  /**
   * The main add-on JS global
   */
  get window() {
    return this.parentActor.global;
  },

  /**
   * Destroy the current AddonConsoleActor instance.
   */
  destroy() {
    Actor.prototype.destroy.call(this);
    WebConsoleActor.prototype.destroy.call(this);
    this.addon = null;
  },

  /**
   * Handler for the "startListeners" request.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response object which holds the startedListeners array.
   */
  startListeners: function ACAOnStartListeners(request) {
    let startedListeners = [];

    while (request.listeners.length > 0) {
      let listener = request.listeners.shift();
      switch (listener) {
        case "ConsoleAPI":
          if (!this.consoleAPIListener) {
            this.consoleAPIListener =
              new ConsoleAPIListener(null, this, { addonId: this.addon.id });
            this.consoleAPIListener.init();
          }
          startedListeners.push(listener);
          break;
      }
    }
    return {
      startedListeners: startedListeners,
      nativeConsoleAPI: true,
      traits: this.traits,
    };
  },
});

exports.AddonConsoleActor = ActorClassWithSpec(webconsoleSpec, addonConsolePrototype);

// TODO: remove once protocol.js can handle inheritance. Bug #1450960
exports.AddonConsoleActor.prototype.typeName = "addonConsole";
