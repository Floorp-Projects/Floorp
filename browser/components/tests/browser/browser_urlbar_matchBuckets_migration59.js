/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure the browser.urlbar.matchBuckets pref is set correctly starting in
// Firefox 59 (nsBrowserGlue UI version 60).

const SEARCHBAR_WIDGET_ID = "search-container";
const PREF_NAME = "browser.urlbar.matchBuckets";
const SEARCHBAR_PRESENT_PREF_VALUE = "general:5,suggestion:Infinity";

add_task(async function test() {
  // Initial checks.
  Assert.equal(CustomizableUI.getPlacementOfWidget(SEARCHBAR_WIDGET_ID), null,
               "Searchbar should not be placed initially");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should be cleared initially");

  // Add the searchbar.
  let widgetPromise = promiseWidget("onWidgetAdded");
  CustomizableUI.addWidgetToArea(SEARCHBAR_WIDGET_ID,
                                 CustomizableUI.AREA_NAVBAR);
  info("Waiting for searchbar to be added");
  await widgetPromise;

  // Force nsBrowserGlue to attempt update the pref again via UI version
  // migration.  It shouldn't actually though since the UI version has already
  // been migrated.  If it erroneously does, then the matchBuckets pref will be
  // set since the searchbar is now placed.
  messageBrowserGlue("force-ui-migration");

  // The pref should remain cleared since the migration already happened even
  // though the searchbar is now present.
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should remain cleared even though searchbar present");

  // Force nsBrowserGlue to update the pref.
  forceBrowserGlueUpdatePref();

  // The pref should be set since the searchbar is present.
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               SEARCHBAR_PRESENT_PREF_VALUE,
               "Pref should be set to show history first");

  // Set the pref to something custom.
  let customValue = "test:Infinity";
  Services.prefs.setCharPref(PREF_NAME, customValue);

  // Force nsBrowserGlue to update the pref again.
  forceBrowserGlueUpdatePref();

  // The pref should remain the custom value.
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), customValue,
               "Pref should remain the custom value");

  // Remove the searchbar.
  widgetPromise = promiseWidget("onWidgetRemoved");
  CustomizableUI.removeWidgetFromArea(SEARCHBAR_WIDGET_ID);
  info("Waiting for searchbar to be removed");
  await widgetPromise;

  // Force nsBrowserGlue to update the pref again.
  forceBrowserGlueUpdatePref();

  // The pref should remain the custom value.
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), customValue,
               "Pref should remain the custom value");

  // Clear the pref.
  Services.prefs.clearUserPref(PREF_NAME);

  // Force nsBrowserGlue to update the pref again.
  forceBrowserGlueUpdatePref();

  // The pref should remain cleared since the searchbar isn't placed.
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should remain cleared");
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

function messageBrowserGlue(msgName) {
  Cc["@mozilla.org/browser/browserglue;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "browser-glue-test", msgName);
}

function forceBrowserGlueUpdatePref() {
  messageBrowserGlue("migrateMatchBucketsPrefForUIVersion60");
}
