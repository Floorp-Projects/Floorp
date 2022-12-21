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

add_task(async function testBfCacheNavigationAfterClosingDevTools() {
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

  // Wait for the iframe target to be processed before destroying the toolbox,
  // to avoid unhandled promise rejections.
  // The iframe URL starts with https://example.org/document-builder.sjs
  let onIframeProcessed;

  // Do not wait for the additional target in the noeft-nofis flavor.
  const isNoEFTNoFis = !isFissionEnabled() && !isEveryFrameTargetEnabled();
  if (!isNoEFTNoFis) {
    const iframeURL = "https://example.org/document-builder.sjs";
    onIframeProcessed = waitForTargetProcessed(toolbox.commands, targetFront =>
      targetFront.url.startsWith(iframeURL)
    );
  }

  info("Navigate to a different URL");
  await navigateTo(TEST_URI + "?someparameter");

  info("Wait for the iframe target to be processed by target-command");
  await onIframeProcessed;

  info("Close DevTools to disable the simulation");
  await toolbox.destroy();
  await waitFor(async () => !(await isSimulationEnabled()));
  is(await isSimulationEnabled(), false, "color scheme simulation is disabled");

  info(
    "Perform a bfcache navigation and check that the simulation is still disabled"
  );
  const awaitPageShow = BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "pageshow"
  );
  gBrowser.goBack();
  await awaitPageShow;

  is(await isSimulationEnabled(), false, "color scheme simulation is disabled");
});

function isSimulationEnabled() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const { matches } = content.matchMedia("(prefers-color-scheme: dark)");
    return matches;
  });
}
