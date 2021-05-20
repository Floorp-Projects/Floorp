/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that maxTouchPoints is updated when touch simulation is toggled.

// TODO: This test should also check that maxTouchPoints is set properly on the iframe.
// This is currently not working and should be worked on in Bug 1706066.

const TEST_DOCUMENT = `doc_with_remote_iframe_and_isolated_cross_origin_capabilities.sjs`;
const TEST_COM_URL = URL_ROOT_COM_SSL + TEST_DOCUMENT;

addRDMTask(TEST_COM_URL, async function({ ui, browser, tab }) {
  info("Toggling on touch simulation.");
  reloadOnTouchChange(true);
  info("Test initial value for maxTouchPoints.");
  is(
    await getMaxTouchPoints(browser),
    0,
    "navigator.maxTouchPoints is 0 when touch simulation is not enabled"
  );

  info(
    "Test value maxTouchPoints is non-zero when touch simulation is enabled."
  );
  await toggleTouchSimulation(ui);
  is(
    await getMaxTouchPoints(browser),
    1,
    "navigator.maxTouchPoints should be 1 after enabling touch simulation"
  );

  info("Toggling off touch simulation.");
  await toggleTouchSimulation(ui);
  is(
    await getMaxTouchPoints(browser),
    0,
    "navigator.maxTouchPoints should be 0 after turning off touch simulation"
  );

  info("Check maxTouchPoints override persists after reload");
  // toggle the touch simulation since it was turned off
  await toggleTouchSimulation(ui);
  let onViewportLoad = waitForViewportLoad(ui);
  browser.reload();
  await onViewportLoad;

  is(
    await getMaxTouchPoints(browser),
    1,
    "navigator.maxTouchPoints is still 1 after reloading"
  );

  info(
    "Check that maxTouchPoints persist after navigating to a page that forces the creation of a new browsing context"
  );
  const previousBrowsingContextId = browser.browsingContext.id;
  const onPageReloaded = BrowserTestUtils.browserLoaded(browser, true);
  onViewportLoad = waitForViewportLoad(ui);
  BrowserTestUtils.loadURI(
    browser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await Promise.all([onPageReloaded, onViewportLoad]);

  isnot(
    browser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    await getMaxTouchPoints(browser),
    1,
    "navigator.maxTouchPoints is still 1 after navigating to a new browsing context"
  );

  info("Check that the value is reset when closing RDM");
  await closeRDM(tab);

  is(
    await getMaxTouchPoints(browser),
    0,
    "navigator.maxTouchPoints is 0 after closing RDM"
  );

  reloadOnTouchChange(false);
});

function getMaxTouchPoints(browserOrBrowsingContext) {
  return SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    return content.navigator.maxTouchPoints;
  });
}
