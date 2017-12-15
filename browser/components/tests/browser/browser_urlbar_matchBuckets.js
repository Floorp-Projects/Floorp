/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure the browser.urlbar.matchBuckets pref is correct and updated when
// the searchbar is added to or removed from the UI.

const SEARCHBAR_WIDGET_ID = "search-container";
const PREF_NAME = "browser.urlbar.matchBuckets";
const SEARCHBAR_PRESENT_PREF_VALUE = "general:5,suggestion:Infinity";

// nsBrowserGlue initializes the relevant code via idleDispatchToMainThread(),
// so to make sure this test runs after that happens, first run a dummy task
// that queues an idle callback.
add_task(async function waitForIdle() {
  await TestUtils.waitForIdle();
});

add_task(async function test() {
  // Initial checks.
  Assert.equal(CustomizableUI.getPlacementOfWidget(SEARCHBAR_WIDGET_ID), null,
               "Sanity check: searchbar should not be placed initially");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Sanity check: pref should be cleared initially");

  // Add the searchbar.  The pref should be set.
  let widgetPromise = promiseWidget("onWidgetAdded");
  CustomizableUI.addWidgetToArea(SEARCHBAR_WIDGET_ID,
                                 CustomizableUI.AREA_NAVBAR);
  info("Waiting for searchbar to be added");
  await widgetPromise;
  Assert.equal(Services.prefs.getCharPref(PREF_NAME),
               SEARCHBAR_PRESENT_PREF_VALUE,
               "Pref should be set after adding searchbar");

  // Remove the searchbar.  The pref should be cleared.
  widgetPromise = promiseWidget("onWidgetRemoved");
  CustomizableUI.removeWidgetFromArea(SEARCHBAR_WIDGET_ID);
  info("Waiting for searchbar to be removed");
  await widgetPromise;
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should be cleared after removing searchbar");

  // Customize the pref.
  let customizedPref = "extension:1," + SEARCHBAR_PRESENT_PREF_VALUE;
  Services.prefs.setCharPref(PREF_NAME, customizedPref);

  // Add the searchbar again.  Since the pref is customized, it shouldn't be
  // changed.
  widgetPromise = promiseWidget("onWidgetAdded");
  CustomizableUI.addWidgetToArea(SEARCHBAR_WIDGET_ID,
                                 CustomizableUI.AREA_NAVBAR);
  info("Waiting for searchbar to be added");
  await widgetPromise;
  Assert.equal(Services.prefs.getCharPref(PREF_NAME), customizedPref,
               "Customized pref should remain same after adding searchbar");

  // Remove the searchbar again.  Since the pref is customized, it shouldn't be
  // changed.
  widgetPromise = promiseWidget("onWidgetRemoved");
  CustomizableUI.removeWidgetFromArea(SEARCHBAR_WIDGET_ID);
  info("Waiting for searchbar to be removed");
  await widgetPromise;
  Assert.equal(Services.prefs.getCharPref(PREF_NAME), customizedPref,
               "Customized pref should remain same after removing searchbar");

  Services.prefs.clearUserPref(PREF_NAME);
});

function promiseWidget(observerName) {
  return new Promise(resolve => {
    let listener = {};
    listener[observerName] = widgetID => {
      if (widgetID == SEARCHBAR_WIDGET_ID) {
        CustomizableUI.removeListener(listener);
        executeSoon(resolve);
      }
    };
    CustomizableUI.addListener(listener);
  });
}
