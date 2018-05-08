/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ConsoleAPIListener } = require("devtools/server/actors/webconsole/listeners");
var { update } = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(this, "WebConsoleActor", "devtools/server/actors/webconsole", true);

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
function AddonConsoleActor(addon, connection, parentActor) {
  this.addon = addon;
  WebConsoleActor.call(this, connection, parentActor);
}

AddonConsoleActor.prototype = Object.create(WebConsoleActor.prototype);

update(AddonConsoleActor.prototype, {
  constructor: AddonConsoleActor,

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
  onStartListeners: function ACAOnStartListeners(request) {
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

AddonConsoleActor.prototype.requestTypes = Object.create(
  WebConsoleActor.prototype.requestTypes
);
AddonConsoleActor.prototype.requestTypes.startListeners =
  AddonConsoleActor.prototype.onStartListeners;

exports.AddonConsoleActor = AddonConsoleActor;
