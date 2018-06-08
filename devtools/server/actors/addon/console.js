/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ConsoleAPIListener } = require("devtools/server/actors/webconsole/listeners");
var { update } = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(this, "WebConsoleActor", "devtools/server/actors/webconsole", true);

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { addonConsoleSpec } = require("devtools/shared/specs/addon/console");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of BrowsingContextTargetActor.prototype
 */
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
 *        The parent AddonTargetActor actor.
 */
addonConsolePrototype.initialize = function(addon, connection, parentActor) {
  this.addon = addon;
  Actor.prototype.initialize.call(this, connection);
  WebConsoleActor.call(this, connection, parentActor);
};

update(addonConsolePrototype, {
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
    const startedListeners = [];

    while (request.listeners.length > 0) {
      const listener = request.listeners.shift();
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

exports.AddonConsoleActor = ActorClassWithSpec(addonConsoleSpec, addonConsolePrototype);
