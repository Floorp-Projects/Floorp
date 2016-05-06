/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Addon Debugger works when devtools.debugger.workers is enabled.
// Workers controller cannot be used when debugging an Addon actor.

const ADDON_URL = EXAMPLE_URL + "addon3.xpi";

function test() {
  Task.spawn(function*() {
    info("Enable worker debugging.");
    yield new Promise(resolve => {
      SpecialPowers.pushPrefEnv({
        "set": [["devtools.debugger.workers", true]]
      }, resolve);
    });

    let addon = yield addAddon(ADDON_URL);
    let addonDebugger = yield initAddonDebugger(ADDON_URL);

    is(addonDebugger.title,
       `Developer Tools - browser_dbg_addon3 - ${ADDON_URL}`,
       "Saw the right toolbox title.");

    info("Check that groups and sources are displayed.");
    let groups = yield addonDebugger.getSourceGroups();
    is(groups.length, 2, "Should be only two groups.");
    let sources = groups[0].sources;
    is(sources.length, 2, "Should be two sources");

    yield addonDebugger.destroy();
    yield removeAddon(addon);
    finish();
  });
}
