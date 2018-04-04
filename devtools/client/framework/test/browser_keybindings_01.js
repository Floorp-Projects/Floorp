/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the keybindings for opening and closing the inspector work as expected
// Can probably make this a shared test that tests all of the tools global keybindings
const TEST_URL = "data:text/html,<html><head><title>Test for the " +
                 "highlighter keybindings</title></head><body>" +
                 "<h1>Keybindings!</h1></body></html>";

const {gDevToolsBrowser} = require("devtools/client/framework/devtools-browser");
let keysetMap = { };

function buildDevtoolsKeysetMap(keyset) {
  [].forEach.call(
    keyset.querySelectorAll("key"),
    function(key) {
      if (!key.getAttribute("key")) {
        return;
      }

      let modifiers = key.getAttribute("modifiers");

      keysetMap[key.id.split("_")[1]] = {
        key: key.getAttribute("key"),
        modifiers: modifiers,
        modifierOpt: {
          shiftKey: modifiers.match("shift"),
          ctrlKey: modifiers.match("ctrl"),
          altKey: modifiers.match("alt"),
          metaKey: modifiers.match("meta"),
          accelKey: modifiers.match("accel")
        },
        synthesizeKey: function() {
          EventUtils.synthesizeKey(this.key, this.modifierOpt);
        }
      };
    });
}

function setupKeyBindingsTest() {
  for (let win of gDevToolsBrowser._trackedBrowserWindows) {
    buildDevtoolsKeysetMap(win.document.getElementById("devtoolsKeyset"));
  }
}

add_task(async function() {
  // Use the new debugger frontend because the old one swallows the netmonitor shortcut:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1370442#c7
  await SpecialPowers.pushPrefEnv({set: [
    ["devtools.debugger.new-debugger-frontend", true]
  ]});

  await addTab(TEST_URL);
  await new Promise(done => waitForFocus(done));

  setupKeyBindingsTest();

  let onToolboxReady = gDevTools.once("toolbox-ready");
  keysetMap.inspector.synthesizeKey();
  let toolbox = await onToolboxReady;

  await inspectorShouldBeOpenAndHighlighting();

  let onSelectTool = gDevTools.once("select-tool-command");
  keysetMap.webconsole.synthesizeKey();
  await onSelectTool;
  await webconsoleShouldBeSelected();

  onSelectTool = gDevTools.once("select-tool-command");
  keysetMap.jsdebugger.synthesizeKey();
  await onSelectTool;
  await jsdebuggerShouldBeSelected();

  onSelectTool = gDevTools.once("select-tool-command");
  keysetMap.netmonitor.synthesizeKey();
  await onSelectTool;
  await netmonitorShouldBeSelected();

  gBrowser.removeCurrentTab();

  async function inspectorShouldBeOpenAndHighlighting() {
    is(toolbox.currentToolId, "inspector", "Correct tool has been loaded");

    await toolbox.once("picker-started");

    ok(true, "picker-started event received, highlighter started");
    keysetMap.inspector.synthesizeKey();

    await toolbox.once("picker-stopped");
    ok(true, "picker-stopped event received, highlighter stopped");
  }

  function webconsoleShouldBeSelected() {
    is(toolbox.currentToolId, "webconsole", "webconsole should be selected.");
  }

  function jsdebuggerShouldBeSelected() {
    is(toolbox.currentToolId, "jsdebugger", "jsdebugger should be selected.");
  }

  function netmonitorShouldBeSelected() {
    is(toolbox.currentToolId, "netmonitor", "netmonitor should be selected.");
  }
});
