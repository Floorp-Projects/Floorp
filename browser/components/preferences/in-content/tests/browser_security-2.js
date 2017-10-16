const PREFS = [
  "browser.safebrowsing.phishing.enabled",
  "browser.safebrowsing.malware.enabled",

  "browser.safebrowsing.downloads.enabled",

  "browser.safebrowsing.downloads.remote.block_potentially_unwanted",
  "browser.safebrowsing.downloads.remote.block_uncommon"
];

let originals = PREFS.map(pref => [pref, Services.prefs.getBoolPref(pref)]);
let originalMalwareTable = Services.prefs.getCharPref("urlclassifier.malwareTable");
registerCleanupFunction(function() {
  originals.forEach(([pref, val]) => Services.prefs.setBoolPref(pref, val));
  Services.prefs.setCharPref("urlclassifier.malwareTable", originalMalwareTable);
});

// This test only opens the Preferences once, and then reloads the page
// each time that it wants to test various preference combinations. We
// only use one tab (instead of opening/closing for each test) for all
// to help improve test times on debug builds.
add_task(async function setup() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  registerCleanupFunction(async function() {
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

// test the download protection preference
add_task(async function() {
  async function checkPrefSwitch(val) {
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", val);

    gBrowser.reload();
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("blockDownloads");
    if (!AppConstants.MOZILLA_OFFICIAL) {
      is(checkbox, undefined, "downloads protection is disabled in un-official builds");
      return;
    }

    let blockUncommon = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(checked, val, "downloads preference is initialized correctly");
    // should be disabled when val is false (= pref is turned off)
    is(blockUncommon.hasAttribute("disabled"), !val, "block uncommon checkbox is set correctly");

    // scroll the checkbox into view, otherwise the synthesizeMouseAtCenter will be ignored, and click it
    checkbox.scrollIntoView();
    EventUtils.synthesizeMouseAtCenter(checkbox, {}, gBrowser.selectedBrowser.contentWindow);

    // check that setting is now turned on or off
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.enabled"), !checked,
       "safebrowsing.downloads preference is set correctly");

    // check if the uncommon warning checkbox has updated
    is(blockUncommon.hasAttribute("disabled"), val, "block uncommon checkbox is set correctly");

  }

  await checkPrefSwitch(true);
  await checkPrefSwitch(false);
});

requestLongerTimeout(2);
// test the unwanted/uncommon software warning preference
add_task(async function() {
  async function checkPrefSwitch(val1, val2, isV2) {
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.remote.block_potentially_unwanted", val1);
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.remote.block_uncommon", val2);
    let testMalwareTable = "goog-malware-" + (isV2 ? "shavar" : "proto");
    testMalwareTable += ",test-malware-simple";
    if (val1 && val2) {
      testMalwareTable += ",goog-unwanted-" + (isV2 ? "shavar" : "proto");
      testMalwareTable += ",test-unwanted-simple";
    }
    Services.prefs.setCharPref("urlclassifier.malwareTable", testMalwareTable);

    gBrowser.reload();
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(checked, val1 && val2, "unwanted/uncommon preference is initialized correctly");

    // scroll the checkbox into view, otherwise the synthesizeMouseAtCenter will be ignored, and click it
    checkbox.scrollIntoView();
    EventUtils.synthesizeMouseAtCenter(checkbox, {}, gBrowser.selectedBrowser.contentWindow);

    // check that both settings are now turned on or off
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.remote.block_potentially_unwanted"), !checked,
       "block_potentially_unwanted is set correctly");
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.remote.block_uncommon"), !checked,
       "block_uncommon is set correctly");

    // when the preference is on, the malware table should include these ids
    let malwareTable = Services.prefs.getCharPref("urlclassifier.malwareTable").split(",");
    if (isV2) {
      is(malwareTable.includes("goog-unwanted-shavar"), !checked,
         "malware table doesn't include goog-unwanted-shavar");
    } else {
      is(malwareTable.includes("goog-unwanted-proto"), !checked,
         "malware table doesn't include goog-unwanted-proto");
    }
    is(malwareTable.includes("test-unwanted-simple"), !checked,
       "malware table doesn't include test-unwanted-simple");
    let sortedMalware = malwareTable.slice(0);
    sortedMalware.sort();
    Assert.deepEqual(malwareTable, sortedMalware, "malware table has been sorted");

  }

  await checkPrefSwitch(true, true, false);
  await checkPrefSwitch(false, true, false);
  await checkPrefSwitch(true, false, false);
  await checkPrefSwitch(false, false, false);
  await checkPrefSwitch(true, true, true);
  await checkPrefSwitch(false, true, true);
  await checkPrefSwitch(true, false, true);
  await checkPrefSwitch(false, false, true);

});
