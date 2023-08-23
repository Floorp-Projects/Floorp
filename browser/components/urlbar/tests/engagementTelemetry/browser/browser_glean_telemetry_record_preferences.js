/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for preference telemetry.

add_setup(async function () {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  // Create a new window in order to initialize TelemetryEvent of
  // UrlbarController.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async function () {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function prefMaxRichResults() {
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Record prefMaxResults when UrlbarController is initialized"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxRichResults", 0]],
  });
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Record prefMaxResults when the maxRichResults pref is updated"
  );
});

add_task(async function boolPref() {
  const testData = [
    {
      green: "prefSuggestDataCollection",
      pref: "quicksuggest.dataCollection.enabled",
    },
    {
      green: "prefSuggestNonsponsored",
      pref: "suggest.quicksuggest.nonsponsored",
    },
    {
      green: "prefSuggestSponsored",
      pref: "suggest.quicksuggest.sponsored",
    },
    {
      green: "prefSuggestTopsites",
      pref: "suggest.topsites",
    },
  ];

  for (const { green, pref } of testData) {
    Assert.equal(
      Glean.urlbar[green].testGetValue(),
      UrlbarPrefs.get(pref),
      `Record ${green} when UrlbarController is initialized`
    );

    await SpecialPowers.pushPrefEnv({
      set: [[`browser.urlbar.${pref}`, !UrlbarPrefs.get(pref)]],
    });

    Assert.equal(
      Glean.urlbar[green].testGetValue(),
      UrlbarPrefs.get(pref),
      `Record ${green} when the ${pref} pref is updated`
    );
  }
});
