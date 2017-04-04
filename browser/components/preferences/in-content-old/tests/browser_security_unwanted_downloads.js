const PREFS = [
  "browser.safebrowsing.phishing.enabled",
  "browser.safebrowsing.malware.enabled",

  "browser.safebrowsing.downloads.enabled",

  "browser.safebrowsing.downloads.remote.block_potentially_unwanted",
  "browser.safebrowsing.downloads.remote.block_uncommon"
];

let originals = PREFS.map(pref => [pref, Services.prefs.getBoolPref(pref)])
let originalMalwareTable = Services.prefs.getCharPref("urlclassifier.malwareTable");
registerCleanupFunction(function() {
  originals.forEach(([pref, val]) => Services.prefs.setBoolPref(pref, val));
  Services.prefs.setCharPref("urlclassifier.malwareTable", originalMalwareTable);
});

// test the unwanted/uncommon software warning preference
add_task(function*() {
  function* checkPrefSwitch(val1, val2) {
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.remote.block_potentially_unwanted", val1);
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.remote.block_uncommon", val2);

    yield openPreferencesViaOpenPreferencesAPI("security", undefined, { leaveOpen: true });

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(checked, val1 && val2, "unwanted/uncommon preference is initialized correctly");

    // click the checkbox
    EventUtils.synthesizeMouseAtCenter(checkbox, {}, gBrowser.selectedBrowser.contentWindow);

    // check that both settings are now turned on or off
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.remote.block_potentially_unwanted"), !checked,
       "block_potentially_unwanted is set correctly");
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.remote.block_uncommon"), !checked,
       "block_uncommon is set correctly");

    // when the preference is on, the malware table should include these ids
    let malwareTable = Services.prefs.getCharPref("urlclassifier.malwareTable").split(",");
    is(malwareTable.includes("goog-unwanted-shavar"), !checked,
       "malware table doesn't include goog-unwanted-shavar");
    is(malwareTable.includes("test-unwanted-simple"), !checked,
       "malware table doesn't include test-unwanted-simple");
    let sortedMalware = malwareTable.slice(0);
    sortedMalware.sort();
    Assert.deepEqual(malwareTable, sortedMalware, "malware table has been sorted");

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  yield* checkPrefSwitch(true, true);
  yield* checkPrefSwitch(false, true);
  yield* checkPrefSwitch(true, false);
  yield* checkPrefSwitch(false, false);
});
