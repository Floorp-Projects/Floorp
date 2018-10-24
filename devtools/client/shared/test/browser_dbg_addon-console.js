/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_addons.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_addons.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { Toolbox } = require("devtools/client/framework/toolbox");
var { Task } = require("devtools/shared/task");

// Test that the we can see console messages from the add-on

const ADDON_ID = "browser_dbg_addon4@tests.mozilla.org";
const ADDON_PATH = "addon4.xpi";

function getCachedMessages(webConsole) {
  const deferred = getDeferredPromise().defer();
  webConsole.getCachedMessages(["ConsoleAPI"], response => {
    if (response.error) {
      deferred.reject(response.error);
      return;
    }
    deferred.resolve(response.messages);
  });
  return deferred.promise;
}

// Creates an add-on debugger for a given add-on. The returned AddonDebugger
// object must be destroyed before finishing the test
function initAddonDebugger(addonId) {
  const addonDebugger = new AddonDebugger();
  return addonDebugger.init(addonId).then(() => addonDebugger);
}

function AddonDebugger() {
  this._onMessage = this._onMessage.bind(this);
  this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
  EventEmitter.decorate(this);
}

AddonDebugger.prototype = {
  init: Task.async(function* (addonId) {
    info("Initializing an addon debugger panel.");

    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    DebuggerServer.allowChromeProcess = true;

    this.frame = document.createElement("iframe");
    this.frame.setAttribute("height", 400);
    document.documentElement.appendChild(this.frame);
    window.addEventListener("message", this._onMessage);

    const transport = DebuggerServer.connectPipe();
    this.client = new DebuggerClient(transport);

    yield this.client.connect();

    const addonTargetActor = yield getAddonActorForId(this.client, addonId);

    const targetOptions = {
      form: addonTargetActor,
      client: this.client,
      chrome: true,
    };

    const toolboxOptions = {
      customIframe: this.frame,
    };

    this.target = yield TargetFactory.forRemoteTab(targetOptions);
    const toolbox = yield gDevTools.showToolbox(
      this.target, "jsdebugger", Toolbox.HostType.CUSTOM, toolboxOptions);

    info("Addon debugger panel shown successfully.");

    this.debuggerPanel = toolbox.getCurrentPanel();
    yield this._attachConsole();
  }),

  destroy: Task.async(function* () {
    yield this.client.close();
    this.frame.remove();
    window.removeEventListener("message", this._onMessage);
  }),

  _attachConsole: function() {
    const deferred = getDeferredPromise().defer();
    this.client.attachConsole(this.target.form.consoleActor, ["ConsoleAPI"])
      .then(([aResponse, aWebConsoleClient]) => {
        this.webConsole = aWebConsoleClient;
        this.client.addListener("consoleAPICall", this._onConsoleAPICall);
        deferred.resolve();
      }, e => {
        deferred.reject(e);
      });
    return deferred.promise;
  },

  _onConsoleAPICall: function(type, packet) {
    if (packet.from != this.webConsole.actor) {
      return;
    }
    this.emit("console", packet.message);
  },

  _onMessage: function(event) {
    if (!event.data) {
      return;
    }
    const msg = event.data;
    switch (msg.name) {
      case "toolbox-title":
        this.title = msg.data.value;
        break;
    }
  },
};

add_task(async function test() {
  const addon = await addTemporaryAddon(ADDON_PATH);
  const addonDebugger = await initAddonDebugger(ADDON_ID);

  const webConsole = addonDebugger.webConsole;
  const messages = await getCachedMessages(webConsole);
  is(messages.length, 1, "Should be one cached message");
  is(messages[0].arguments[0].type, "object", "Should have logged an object");
  is(messages[0].arguments[0].preview.ownProperties.msg.value,
    "Hello from the test add-on", "Should have got the right message");

  const consolePromise = addonDebugger.once("console");

  console.log("Bad message");
  Services.obs.notifyObservers(null, "addon-test-ping");

  const messageGrip = await consolePromise;
  is(messageGrip.arguments[0].type, "object", "Should have logged an object");
  is(messageGrip.arguments[0].preview.ownProperties.msg.value,
    "Hello again", "Should have got the right message");

  await addonDebugger.destroy();
  await removeAddon(addon);
  finish();
});
