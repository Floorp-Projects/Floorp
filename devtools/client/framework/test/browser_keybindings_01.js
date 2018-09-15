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
ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
const isMac = AppConstants.platform == "macosx";

const allKeys = [];
function buildDevtoolsKeysetMap(keyset) {
  // Fetches all the keyboard shortcuts which were defined by lazyGetter 'KeyShortcuts' in
  // devtools-startup.js and added to the DOM by 'hookKeyShortcuts'
  [...keyset.querySelectorAll("key")].forEach(key => {
    if (!key.getAttribute("key")) {
      return;
    }

    const modifiers = key.getAttribute("modifiers");
    allKeys.push({
      toolId: key.id.split("_")[1],
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
    });
  });
}

function setupKeyBindingsTest() {
  for (const win of gDevToolsBrowser._trackedBrowserWindows) {
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

  info("Test the first inspector key (there are 2 of them on Mac)");
  const inspectorKeys = allKeys.filter(({ toolId }) => {
    return toolId === "inspector" || toolId === "inspectorMac";
  });

  if (isMac) {
    is(inspectorKeys.length, 2, "There are 2 inspector keys on Mac");
  } else {
    is(inspectorKeys.length, 1, "Only 1 inspector key on non-Mac platforms");
  }

  info("The first inspector key should open the toolbox.");
  const onToolboxReady = gDevTools.once("toolbox-ready");
  inspectorKeys[0].synthesizeKey();
  const toolbox = await onToolboxReady;
  await inspectorShouldBeOpenAndHighlighting(inspectorKeys[0]);

  let onSelectTool = gDevTools.once("select-tool-command");
  const webconsole = allKeys.filter(({ toolId }) => toolId === "webconsole")[0];
  webconsole.synthesizeKey();
  await onSelectTool;
  await webconsoleShouldBeSelected();

  onSelectTool = gDevTools.once("select-tool-command");
  const jsdebugger = allKeys.filter(({ toolId }) => toolId === "jsdebugger")[0];
  jsdebugger.synthesizeKey();
  await onSelectTool;
  await jsdebuggerShouldBeSelected();

  onSelectTool = gDevTools.once("select-tool-command");
  const netmonitor = allKeys.filter(({ toolId }) => toolId === "netmonitor")[0];
  netmonitor.synthesizeKey();
  await onSelectTool;
  await netmonitorShouldBeSelected();

  if (isMac) {
    info("On MacOS, we check the extra inspector shortcut too");
    onSelectTool = gDevTools.once("select-tool-command");
    inspectorKeys[1].synthesizeKey();
    await onSelectTool;
    await inspectorShouldBeOpenAndHighlighting(inspectorKeys[1]);
  }

  gBrowser.removeCurrentTab();

  async function inspectorShouldBeOpenAndHighlighting(inspector) {
    is(toolbox.currentToolId, "inspector", "Correct tool has been loaded");

    await toolbox.once("picker-started");

    ok(true, "picker-started event received, highlighter started");
    inspector.synthesizeKey();

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
