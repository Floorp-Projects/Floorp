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

// test the download protection preference
add_task(function*() {
  function* checkPrefSwitch(val) {
    Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", val);

    yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("blockDownloads");
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

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  yield* checkPrefSwitch(true);
  yield* checkPrefSwitch(false);
});
