/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from shared-head.js */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js", this);

function toggleAllTools(state) {
  for (let [, tool] of gDevTools._tools) {
    if (!tool.visibilityswitch) {
      continue;
    }
    if (state) {
      Services.prefs.setBoolPref(tool.visibilityswitch, true);
    } else {
      Services.prefs.clearUserPref(tool.visibilityswitch);
    }
  }
}

function getChromeActors(callback)
{
  let { DebuggerServer } = require("devtools/server/main");
  let { DebuggerClient } = require("devtools/shared/client/main");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect()
    .then(() => client.getProcess())
    .then(response => {
      callback(client, response.form);
    });

  SimpleTest.registerCleanupFunction(() => {
    DebuggerServer.destroy();
  });
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

/**
 * Open a Scratchpad window.
 *
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad.
 */
function* openScratchpadWindow() {
  let { promise: p, resolve } = defer();
  let win = ScratchpadManager.openScratchpad();

  yield once(win, "load");

  win.Scratchpad.addObserver({
    onReady: function () {
      win.Scratchpad.removeObserver(this);
      resolve(win);
    }
  });
  return p;
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  info("Expecting message " + name + " from content");

  let mm = gBrowser.selectedBrowser.messageManager;

  let def = defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg.data);
  });
  return def.promise;
}

/**
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 * @param {String} name The message name. Should be one of the messages defined
 * in doc_frame_script.js
 * @param {Object} data Optional data to send along
 * @param {Object} objects Optional CPOW objects to send along
 * @param {Boolean} expectResponse If set to false, don't wait for a response
 * with the same name from the content script. Defaults to true.
 * @return {Promise} Resolves to the response data if a response is expected,
 * immediately resolves otherwise
 */
function executeInContent(name, data = {}, objects = {}, expectResponse = true) {
  info("Sending message " + name + " to content");
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  } else {
    return promise.resolve();
  }
}

/**
 * Synthesize a keypress from a <key> element, taking into account
 * any modifiers.
 * @param {Element} el the <key> element to synthesize
 */
function synthesizeKeyElement(el) {
  let key = el.getAttribute("key") || el.getAttribute("keycode");
  let mod = {};
  el.getAttribute("modifiers").split(" ").forEach((m) => mod[m + "Key"] = true);
  info(`Synthesizing: key=${key}, mod=${JSON.stringify(mod)}`);
  EventUtils.synthesizeKey(key, mod, el.ownerDocument.defaultView);
}

/* Check the toolbox host type and prefs to make sure they match the
 * expected values
 * @param {Toolbox}
 * @param {HostType} hostType
 *        One of {SIDE, BOTTOM, WINDOW} from Toolbox.HostType
 * @param {HostType} Optional previousHostType
 *        The host that will be switched to when calling switchToPreviousHost
 */
function checkHostType(toolbox, hostType, previousHostType) {
  is(toolbox.hostType, hostType, "host type is " + hostType);

  let pref = Services.prefs.getCharPref("devtools.toolbox.host");
  is(pref, hostType, "host pref is " + hostType);

  if (previousHostType) {
    is(Services.prefs.getCharPref("devtools.toolbox.previousHost"),
      previousHostType, "The previous host is correct");
  }
}
