/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that the sources listed when debugging an addon are either from the
// addon itself, or the SDK, with proper groups and labels.

const ADDON_URL = EXAMPLE_URL + "addon3.xpi";
var gClient;

function test() {
  Task.spawn(function*() {
    let addon = yield addAddon(ADDON_URL);
    let addonDebugger = yield initAddonDebugger(ADDON_URL);

    is(addonDebugger.title, "Debugger - browser_dbg_addon3", "Saw the right toolbox title.");

    // Check the inital list of sources is correct
    let groups = yield addonDebugger.getSourceGroups();
    is(groups[0].name, "jid1-ami3akps3baaeg@jetpack", "Add-on code should be the first group");
    is(groups[1].name, "Add-on SDK", "Add-on SDK should be the second group");
    is(groups.length, 2, "Should be only two groups.");

    let sources = groups[0].sources;
    is(sources.length, 2, "Should be two sources");
    ok(sources[0].url.endsWith("/jid1-ami3akps3baaeg@jetpack.xpi!/bootstrap.js"), "correct url for bootstrap code")
    is(sources[0].label, "bootstrap.js", "correct label for bootstrap code")
    is(sources[1].url, "resource://jid1-ami3akps3baaeg-at-jetpack/browser_dbg_addon3/lib/main.js", "correct url for add-on code")
    is(sources[1].label, "resources/browser_dbg_addon3/lib/main.js", "correct label for add-on code")

    ok(groups[1].sources.length > 10, "SDK modules are listed");

    yield addonDebugger.destroy();
    yield removeAddon(addon);
    finish();
  });
}
