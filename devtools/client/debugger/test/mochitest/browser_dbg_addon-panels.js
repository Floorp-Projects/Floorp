/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that only panels that are relevant to the addon debugger
// display in the toolbox

const ADDON_ID = "jid1-ami3akps3baaeg@jetpack";
const ADDON_PATH = "addon3.xpi";

var gAddon, gClient, gThreadClient, gDebugger, gSources;
var PREFS = [
  ["devtools.canvasdebugger.enabled", true],
  ["devtools.shadereditor.enabled", true],
  ["devtools.performance.enabled", true],
  ["devtools.netmonitor.enabled", true],
  ["devtools.scratchpad.enabled", true]
];
function test() {
  Task.spawn(function* () {
    // Store and enable all optional dev tools panels
    yield pushPrefs(...PREFS);

    let addon = yield addTemporaryAddon(ADDON_PATH);
    let addonDebugger = yield initAddonDebugger(ADDON_ID);

    // Check only valid tabs are shown
    let tabs = addonDebugger.frame.contentDocument.getElementById("toolbox-tabs").children;
    let expectedTabs = ["webconsole", "jsdebugger", "scratchpad"];

    is(tabs.length, expectedTabs.length, "displaying only " + expectedTabs.length + " tabs in addon debugger");
    Array.forEach(tabs, (tab, i) => {
      let toolName = expectedTabs[i];
      is(tab.getAttribute("toolid"), toolName, "displaying " + toolName);
    });

    // Check no toolbox buttons are shown
    let buttons = addonDebugger.frame.contentDocument.getElementById("toolbox-buttons").children;
    Array.forEach(buttons, (btn, i) => {
      is(btn.hidden, true, "no toolbox buttons for the addon debugger -- " + btn.className);
    });

    yield addonDebugger.destroy();
    yield removeAddon(addon);

    finish();
  });
}
