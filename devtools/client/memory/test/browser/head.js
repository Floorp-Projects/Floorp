/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the shared test helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

Services.prefs.setBoolPref("devtools.memory.enabled", true);

/**
 * Open the memory panel for the given tab.
 */
this.openMemoryPanel = Task.async(function* (tab) {
  info("Opening memory panel.");
  const target = TargetFactory.forTab(tab);
  const toolbox = yield gDevTools.showToolbox(target, "memory");
  info("Memory panel shown successfully.");
  let panel = toolbox.getCurrentPanel();
  return { tab, panel };
});

/**
 * Close the memory panel for the given tab.
 */
this.closeMemoryPanel = Task.async(function* (tab) {
  info("Closing memory panel.");
  const target = TargetFactory.forTab(tab);
  const toolbox = gDevTools.getToolbox(target);
  yield toolbox.destroy();
  info("Closed memory panel successfully.");
});

/**
 * Return a test function that adds a tab with the given url, opens the memory
 * panel, runs the given generator, closes the memory panel, removes the tab,
 * and finishes.
 *
 * Example usage:
 *
 *     this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
 *         // Your tests go here...
 *     });
 */
function makeMemoryTest(url, generator) {
  return Task.async(function* () {
    waitForExplicitFinish();

    const tab = yield addTab(url);
    const results = yield openMemoryPanel(tab);

    try {
      yield* generator(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    yield closeMemoryPanel(tab);
    yield removeTab(tab);

    finish();
  });
}
