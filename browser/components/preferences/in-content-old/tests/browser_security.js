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

// test the safebrowsing preference
add_task(function*() {
  function* checkPrefSwitch(val1, val2) {
    Services.prefs.setBoolPref("browser.safebrowsing.phishing.enabled", val1);
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
    is(Services.prefs.getBoolPref("browser.safebrowsing.phishing.enabled"), !checked,
       "safebrowsing.enabled is set correctly");
    is(Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled"), !checked,
       "safebrowsing.malware.enabled is set correctly");

    // check if the other checkboxes have updated
    checked = checkbox.checked;
    is(blockDownloads.hasAttribute("disabled"), !checked, "block downloads checkbox is set correctly");
    is(blockUncommon.hasAttribute("disabled"), !checked || !blockDownloads.checked, "block uncommon checkbox is set correctly");

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  yield* checkPrefSwitch(true, true);
  yield* checkPrefSwitch(false, true);
  yield* checkPrefSwitch(true, false);
  yield* checkPrefSwitch(false, false);
});
