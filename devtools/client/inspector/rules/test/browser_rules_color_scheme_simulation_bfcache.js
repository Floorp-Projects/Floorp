/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation.
const TEST_URI = URL_ROOT_SSL + "doc_media_queries.html";

add_task(async function testBfCacheNavigationWithDevTools() {
  await addTab(TEST_URI);
  const { inspector, toolbox } = await openRuleView();

  is(await isSimulationEnabled(), false, "color scheme simulation is disabled");

  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );
  ok(darkButton, "The dark color scheme simulation button exists");

  info("Click on the dark button");
  darkButton.click();
  await waitFor(async () => isSimulationEnabled());
  is(await isSimulationEnabled(), true, "color scheme simulation is enabled");

  info("Navigate to a different URL and disable the color simulation");
  await navigateTo(TEST_URI + "?someparameter");
  darkButton.click();
  await waitFor(async () => !(await isSimulationEnabled()));
  is(await isSimulationEnabled(), false, "color scheme simulation is disabled");

  info(
    "Perform a bfcache navigation and check that the simulation is still disabled"
  );
  const waitForDevToolsReload = await watchForDevToolsReload(
    gBrowser.selectedBrowser
  );
  gBrowser.goBack();
  await waitForDevToolsReload();
  is(await isSimulationEnabled(), false, "color scheme simulation is disabled");

  await toolbox.destroy();
});

function isSimulationEnabled() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const { matches } = content.matchMedia("(prefers-color-scheme: dark)");
    return matches;
  });
}
