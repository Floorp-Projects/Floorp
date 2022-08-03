/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

// Tests that the keybindings for opening and closing the inspector work as expected
// Can probably make this a shared test that tests all of the tools global keybindings
const TEST_URL =
  "data:text/html,<html><head><title>Test for the " +
  "highlighter keybindings</title></head><body>" +
  "<h1>Keybindings!</h1></body></html>";

const {
  gDevToolsBrowser,
} = require("devtools/client/framework/devtools-browser");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
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
      modifiers,
      modifierOpt: {
        shiftKey: modifiers.match("shift"),
        ctrlKey: modifiers.match("ctrl"),
        altKey: modifiers.match("alt"),
        metaKey: modifiers.match("meta"),
        accelKey: modifiers.match("accel"),
      },
      synthesizeKey() {
        EventUtils.synthesizeKey(this.key, this.modifierOpt);
      },
    });
  });
}

function setupKeyBindingsTest() {
  for (const win of gDevToolsBrowser._trackedBrowserWindows) {
    buildDevtoolsKeysetMap(win.document.getElementById("devtoolsKeyset"));
  }
}

add_task(async function() {
  await addTab(TEST_URL);
  await new Promise(done => waitForFocus(done));

  setupKeyBindingsTest();

  const tests = [
    { id: "inspector", toolId: "inspector" },
    { id: "webconsole", toolId: "webconsole" },
    { id: "netmonitor", toolId: "netmonitor" },
    { id: "jsdebugger", toolId: "jsdebugger" },
  ];

  // There are two possible keyboard shortcuts to open the inspector on macOS
  if (isMac) {
    tests.push({ id: "inspectorMac", toolId: "inspector" });
  }

  // Toolbox reference will be set by first tool to open.
  let toolbox;

  for (const test of tests) {
    const onToolboxReady = gDevTools.once("toolbox-ready");
    const onSelectTool = gDevTools.once("select-tool-command");

    info(`Run the keyboard shortcut for ${test.id}`);
    const key = allKeys.filter(({ toolId }) => toolId === test.id)[0];
    key.synthesizeKey();

    if (!toolbox) {
      toolbox = await onToolboxReady;
    }

    if (test.toolId === "inspector") {
      const onPickerStart = toolbox.nodePicker.once("picker-started");
      await onPickerStart;
      ok(true, "picker-started event received, highlighter started");

      info(
        `Run the keyboard shortcut for ${test.id} again to stop the node picker`
      );
      const onPickerStop = toolbox.nodePicker.once("picker-stopped");
      key.synthesizeKey();
      await onPickerStop;
      ok(true, "picker-stopped event received, highlighter stopped");
    }

    await onSelectTool;
    is(toolbox.currentToolId, test.toolId, `${test.toolId} should be selected`);
  }

  gBrowser.removeCurrentTab();
});
