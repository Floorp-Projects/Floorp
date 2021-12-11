"use strict";

var gTestTab;
var gContentAPI;

requestLongerTimeout(2);
add_task(setup_UITourTest);

add_UITour_task(async function test_bg_getConfiguration() {
  info("getConfiguration is on the allowed list so should work");
  await loadForegroundTab();
  let data = await getConfigurationPromise("availableTargets");
  ok(data, "Got data from getConfiguration");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_UITour_task(async function test_bg_showInfo() {
  info("showInfo isn't on the allowed action list so should be denied");
  await loadForegroundTab();

  await showInfoPromise(
    "appMenu",
    "Hello from the background",
    "Surprise!"
  ).then(
    () => ok(false, "panel shouldn't have shown from a background tab"),
    () => ok(true, "panel wasn't shown from a background tab")
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function loadForegroundTab() {
  // Spawn a content task that resolves once we're sure the visibilityState was
  // changed. This state is what the tests in this file rely on.
  let promise = SpecialPowers.spawn(
    gBrowser.selectedTab.linkedBrowser,
    [],
    async function() {
      return new Promise(resolve => {
        let document = content.document;
        document.addEventListener("visibilitychange", function onStateChange() {
          Assert.equal(
            document.visibilityState,
            "hidden",
            "UITour page should be hidden now."
          );
          document.removeEventListener("visibilitychange", onStateChange);
          resolve();
        });
      });
    }
  );
  await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await promise;
  isnot(gBrowser.selectedTab, gTestTab, "Make sure tour tab isn't selected");
}
