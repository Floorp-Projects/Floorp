const PREFS = [
  "browser.safebrowsing.enabled",
  "browser.safebrowsing.malware.enabled",

  "browser.safebrowsing.downloads.enabled",

  "browser.safebrowsing.downloads.remote.block_potentially_unwanted",
  "browser.safebrowsing.downloads.remote.block_uncommon"
];

let originals = PREFS.map(pref => [pref, Services.prefs.getBoolPref(pref)])
let originalMalwareTable = Services.prefs.getCharPref("urlclassifier.malwareTable");
registerCleanupFunction(function() {
  originals.forEach(([pref, val]) => Services.prefs.setBoolPref(pref, val))
  Services.prefs.setCharPref("urlclassifier.malwareTable", originalMalwareTable);
});

// test the safebrowsing preference
add_task(function*() {
  function* checkPrefSwitch(val1, val2) {
    Services.prefs.setBoolPref("browser.safebrowsing.enabled", val1);
    Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", val2);

    yield openPreferencesViaOpenPreferencesAPI("security", undefined, { leaveOpen: true });

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("enableSafeBrowsing");
    let blockDownloads = doc.getElementById("blockDownloads");
    let blockUncommon = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(checked, val1 && val2, "safebrowsing preference is initialized correctly");
    // should be disabled when checked is false (= pref is turned off)
    is(blockDownloads.hasAttribute("disabled"), !checked, "block downloads checkbox is set correctly");
    is(blockUncommon.hasAttribute("disabled"), !checked, "block uncommon checkbox is set correctly");

    // click the checkbox
    EventUtils.synthesizeMouseAtCenter(checkbox, {}, gBrowser.selectedBrowser.contentWindow);

    // check that both settings are now turned on or off
    is(Services.prefs.getBoolPref("browser.safebrowsing.enabled"), !checked,
       "safebrowsing.enabled is set correctly");
    is(Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled"), !checked,
       "safebrowsing.malware.enabled is set correctly");

    // check if the other checkboxes have updated
    checked = checkbox.checked;
    is(blockDownloads.hasAttribute("disabled"), !checked, "block downloads checkbox is set correctly");
    is(blockUncommon.hasAttribute("disabled"), !checked || !blockDownloads.checked, "block uncommon checkbox is set correctly");

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  yield checkPrefSwitch(true, true);
  yield checkPrefSwitch(false, true);
  yield checkPrefSwitch(true, false);
  yield checkPrefSwitch(false, false);
});

// test the download protection preference
add_task(function*() {
  function* checkPrefSwitch(val) {
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", val);

    yield openPreferencesViaOpenPreferencesAPI("security", undefined, { leaveOpen: true });

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("blockDownloads");
    let blockUncommon = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(checked, val, "downloads preference is initialized correctly");
    // should be disabled when val is false (= pref is turned off)
    is(blockUncommon.hasAttribute("disabled"), !val, "block uncommon checkbox is set correctly");

    // click the checkbox
    EventUtils.synthesizeMouseAtCenter(checkbox, {}, gBrowser.selectedBrowser.contentWindow);

    // check that setting is now turned on or off
    is(Services.prefs.getBoolPref("browser.safebrowsing.downloads.enabled"), !checked,
       "safebrowsing.downloads preference is set correctly");

    // check if the uncommon warning checkbox has updated
    is(blockUncommon.hasAttribute("disabled"), val, "block uncommon checkbox is set correctly");

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  yield checkPrefSwitch(true);
  yield checkPrefSwitch(false);
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

  yield checkPrefSwitch(true, true);
  yield checkPrefSwitch(false, true);
  yield checkPrefSwitch(true, false);
  yield checkPrefSwitch(false, false);
});
