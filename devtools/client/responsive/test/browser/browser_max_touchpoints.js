/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that maxTouchPoints is updated when touch simulation is toggled.

// TODO: This test should also check that maxTouchPoints is set properly on the iframe.
// This is currently not working and should be worked on in Bug 1706066.

const TEST_DOCUMENT = `doc_with_remote_iframe_and_isolated_cross_origin_capabilities.sjs`;
const TEST_COM_URL = URL_ROOT_COM_SSL + TEST_DOCUMENT;

addRDMTask(TEST_COM_URL, async function({ ui, browser, tab }) {
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
  await toggleTouchSimulation(ui);

  let onPageReloaded = BrowserTestUtils.browserLoaded(browser, true);
  browser.reload();
  await onPageReloaded;

  is(
    await getMaxTouchPoints(browser),
    1,
    "navigator.maxTouchPoints is still 1 after reloading"
  );

  info(
    "Check that maxTouchPoints persist after navigating to a page that forces the creation of a new browsing context"
  );
  const previousBrowsingContextId = browser.browsingContext.id;
  onPageReloaded = BrowserTestUtils.browserLoaded(browser, true, loadedUrl =>
    loadedUrl.includes(URL_ROOT_ORG_SSL)
  );

  // `closeRDM`, which is used later, check that the responsiveFront isn't destroyed
  // to reset the state of the document. It might happen that the rest of the test
  // happens so fast that the RDM ui doesn't have the time to update its responsiveFront
  // instance (it's done onTargetAvailable), and so the one it will check is the "old" one,
  // which is likely to be destroyed, causing the destroy method to not be fully executed
  // as expected. In order to prevent such thing, we do wait until a new instance is
  // put on the ui.
  let targets = 0;
  const expectedTargets = isFissionEnabled() ? 2 : 1;
  const onAvailableTargetProcessed = new Promise(resolve => {
    const off = ui.commands.targetCommand.on(
      "processed-available-target",
      () => {
        targets++;
        if (targets == expectedTargets) {
          resolve();
          off();
        }
      }
    );
  });

  BrowserTestUtils.loadURI(
    browser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await Promise.all([onPageReloaded, onAvailableTargetProcessed]);

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
  // Closing RDM trigers a reload
  onPageReloaded = BrowserTestUtils.browserLoaded(browser, true);
  await closeRDM(tab);
  await onPageReloaded;

  is(
    await getMaxTouchPoints(browser),
    0,
    "navigator.maxTouchPoints is 0 after closing RDM"
  );

  reloadOnTouchChange(false);
});

function getMaxTouchPoints(browserOrBrowsingContext) {
  return ContentTask.spawn(
    browserOrBrowsingContext,
    null,
    () => content.navigator.maxTouchPoints
  );
}
