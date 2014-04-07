/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that only panels that are relevant to the addon debugger
// display in the toolbox

const ADDON_URL = EXAMPLE_URL + "addon3.xpi";

let gAddon, gClient, gThreadClient, gDebugger, gSources;
let PREFS = [
  "devtools.canvasdebugger.enabled",
  "devtools.shadereditor.enabled",
  "devtools.profiler.enabled",
  "devtools.netmonitor.enabled"
];
function test() {
  Task.spawn(function () {
    let addon = yield addAddon(ADDON_URL);
    let addonDebugger = yield initAddonDebugger(ADDON_URL);

    // Store and enable all optional dev tools panels
    let originalPrefs = PREFS.map(pref => {
      let original = Services.prefs.getBoolPref(pref);
      Services.prefs.setBoolPref(pref, true)
      return original;
    });

    let tabs = addonDebugger.frame.contentDocument.getElementById("toolbox-tabs").children;
    let expectedTabs = ["options", "jsdebugger"];

    is(tabs.length, 2, "displaying only 2 tabs in addon debugger");
    Array.forEach(tabs, (tab, i) => {
      let toolName = expectedTabs[i];
      is(tab.getAttribute("toolid"), toolName, "displaying " + toolName);
    });

    yield addonDebugger.destroy();
    yield removeAddon(addon);

    PREFS.forEach((pref, i) => Services.prefs.setBoolPref(pref, originalPrefs[i]));

    finish();
  });
}
