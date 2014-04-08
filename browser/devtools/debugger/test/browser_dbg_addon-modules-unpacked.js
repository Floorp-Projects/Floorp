/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure the add-on actor can see loaded JS Modules from an add-on

const ADDON_URL = EXAMPLE_URL + "addon5.xpi";

function test() {
  Task.spawn(function () {
    let addon = yield addAddon(ADDON_URL);
    let addonDebugger = yield initAddonDebugger(ADDON_URL);

    is(addonDebugger.title, "Debugger - Test unpacked add-on with JS Modules", "Saw the right toolbox title.");

    // Check the inital list of sources is correct
    let groups = yield addonDebugger.getSourceGroups();
    is(groups[0].name, "browser_dbg_addon5@tests.mozilla.org", "Add-on code should be the first group");
    is(groups.length, 1, "Should be only one group.");

    let sources = groups[0].sources;
    is(sources.length, 2, "Should be two sources");
    ok(sources[0].url.endsWith("/browser_dbg_addon5@tests.mozilla.org/bootstrap.js"), "correct url for bootstrap code")
    is(sources[0].label, "bootstrap.js", "correct label for bootstrap code")
    is(sources[1].url, "resource://browser_dbg_addon5/test.jsm", "correct url for addon code")
    is(sources[1].label, "test.jsm", "correct label for addon code")

    // Load a new module and check it appears in the list of sources
    Cu.import("resource://browser_dbg_addon5/test2.jsm", {});

    groups = yield addonDebugger.getSourceGroups();
    is(groups[0].name, "browser_dbg_addon5@tests.mozilla.org", "Add-on code should be the first group");
    is(groups.length, 1, "Should be only one group.");

    sources = groups[0].sources;
    is(sources.length, 3, "Should be three sources");
    ok(sources[0].url.endsWith("/browser_dbg_addon5@tests.mozilla.org/bootstrap.js"), "correct url for bootstrap code")
    is(sources[0].label, "bootstrap.js", "correct label for bootstrap code")
    is(sources[1].url, "resource://browser_dbg_addon5/test.jsm", "correct url for addon code")
    is(sources[1].label, "test.jsm", "correct label for addon code")
    is(sources[2].url, "resource://browser_dbg_addon5/test2.jsm", "correct url for addon code")
    is(sources[2].label, "test2.jsm", "correct label for addon code")

    Cu.unload("resource://browser_dbg_addon5/test2.jsm");
    yield addonDebugger.destroy();
    yield removeAddon(addon);
    finish();
  });
}
