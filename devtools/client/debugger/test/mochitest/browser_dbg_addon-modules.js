/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure the add-on actor can see loaded JS Modules from an add-on

const ADDON_ID = "browser_dbg_addon4@tests.mozilla.org";
const ADDON_PATH = "addon4.xpi";
const ADDON_URL = getTemporaryAddonURLFromPath(ADDON_PATH);

function test() {
  Task.spawn(function* () {
    let addon = yield addTemporaryAddon(ADDON_PATH);
    let tab1 = yield addTab("chrome://browser_dbg_addon4/content/test.xul");

    let addonDebugger = yield initAddonDebugger(ADDON_ID);

    is(addonDebugger.title, `Developer Tools - Test add-on with JS Modules - ${ADDON_URL}`,
       "Saw the right toolbox title.");

    // Check the inital list of sources is correct
    let groups = yield addonDebugger.getSourceGroups();
    is(groups[0].name, "browser_dbg_addon4@tests.mozilla.org", "Add-on code should be the first group");
    is(groups[1].name, "chrome://global", "XUL code should be the second group");
    is(groups.length, 2, "Should be only two groups.");

    let sources = groups[0].sources;
    is(sources.length, 3, "Should be three sources");
    ok(sources[0].url.endsWith("/addon4.xpi!/bootstrap.js"), "correct url for bootstrap code");
    is(sources[0].label, "bootstrap.js", "correct label for bootstrap code");
    is(sources[1].url, "resource://browser_dbg_addon4/test.jsm", "correct url for addon code");
    is(sources[1].label, "test.jsm", "correct label for addon code");
    is(sources[2].url, "chrome://browser_dbg_addon4/content/testxul.js", "correct url for addon tab code");
    is(sources[2].label, "testxul.js", "correct label for addon tab code");

    // Load a new module and tab and check they appear in the list of sources
    Cu.import("resource://browser_dbg_addon4/test2.jsm", {});
    let tab2 = yield addTab("chrome://browser_dbg_addon4/content/test2.xul");

    groups = yield addonDebugger.getSourceGroups();
    is(groups[0].name, "browser_dbg_addon4@tests.mozilla.org", "Add-on code should be the first group");
    is(groups[1].name, "chrome://global", "XUL code should be the second group");
    is(groups.length, 2, "Should be only two groups.");

    sources = groups[0].sources;
    is(sources.length, 5, "Should be five sources");
    ok(sources[0].url.endsWith("/addon4.xpi!/bootstrap.js"), "correct url for bootstrap code");
    is(sources[0].label, "bootstrap.js", "correct label for bootstrap code");
    is(sources[1].url, "resource://browser_dbg_addon4/test.jsm", "correct url for addon code");
    is(sources[1].label, "test.jsm", "correct label for addon code");
    is(sources[2].url, "chrome://browser_dbg_addon4/content/testxul.js", "correct url for addon tab code");
    is(sources[2].label, "testxul.js", "correct label for addon tab code");
    is(sources[3].url, "resource://browser_dbg_addon4/test2.jsm", "correct url for addon code");
    is(sources[3].label, "test2.jsm", "correct label for addon code");
    is(sources[4].url, "chrome://browser_dbg_addon4/content/testxul2.js", "correct url for addon tab code");
    is(sources[4].label, "testxul2.js", "correct label for addon tab code");

    Cu.unload("resource://browser_dbg_addon4/test2.jsm");
    yield addonDebugger.destroy();
    yield removeTab(tab1);
    yield removeTab(tab2);
    yield removeAddon(addon);
    finish();
  });
}
