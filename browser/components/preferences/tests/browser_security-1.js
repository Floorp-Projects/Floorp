const PREFS = [
  "browser.safebrowsing.phishing.enabled",
  "browser.safebrowsing.malware.enabled",

  "browser.safebrowsing.downloads.enabled",

  "browser.safebrowsing.downloads.remote.block_potentially_unwanted",
  "browser.safebrowsing.downloads.remote.block_uncommon",
];

let originals = PREFS.map(pref => [pref, Services.prefs.getBoolPref(pref)]);
let originalMalwareTable = Services.prefs.getCharPref(
  "urlclassifier.malwareTable"
);
registerCleanupFunction(function() {
  originals.forEach(([pref, val]) => Services.prefs.setBoolPref(pref, val));
  Services.prefs.setCharPref(
    "urlclassifier.malwareTable",
    originalMalwareTable
  );
});

// This test only opens the Preferences once, and then reloads the page
// each time that it wants to test various preference combinations. We
// only use one tab (instead of opening/closing for each test) for all
// to help improve test times on debug builds.
add_task(async function setup() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

// test the safebrowsing preference
add_task(async function() {
  async function checkPrefSwitch(val1, val2) {
    Services.prefs.setBoolPref("browser.safebrowsing.phishing.enabled", val1);
    Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", val2);

    gBrowser.reload();
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    let doc = gBrowser.selectedBrowser.contentDocument;
    let checkbox = doc.getElementById("enableSafeBrowsing");
    let blockDownloads = doc.getElementById("blockDownloads");
    let blockUncommon = doc.getElementById("blockUncommonUnwanted");
    let checked = checkbox.checked;
    is(
      blockDownloads.hasAttribute("disabled"),
      !checked,
      "block downloads checkbox is set correctly"
    );

    is(
      checked,
      val1 && val2,
      "safebrowsing preference is initialized correctly"
    );
    // should be disabled when checked is false (= pref is turned off)
    is(
      blockUncommon.hasAttribute("disabled"),
      !checked,
      "block uncommon checkbox is set correctly"
    );

    // scroll the checkbox into the viewport and click checkbox
    checkbox.scrollIntoView();
    EventUtils.synthesizeMouseAtCenter(
      checkbox,
      {},
      gBrowser.selectedBrowser.contentWindow
    );

    // check that both settings are now turned on or off
    is(
      Services.prefs.getBoolPref("browser.safebrowsing.phishing.enabled"),
      !checked,
      "safebrowsing.enabled is set correctly"
    );
    is(
      Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled"),
      !checked,
      "safebrowsing.malware.enabled is set correctly"
    );

    // check if the other checkboxes have updated
    checked = checkbox.checked;
    if (blockDownloads) {
      is(
        blockDownloads.hasAttribute("disabled"),
        !checked,
        "block downloads checkbox is set correctly"
      );
      is(
        blockUncommon.hasAttribute("disabled"),
        !checked || !blockDownloads.checked,
        "block uncommon checkbox is set correctly"
      );
    }
  }

  await checkPrefSwitch(true, true);
  await checkPrefSwitch(false, true);
  await checkPrefSwitch(true, false);
  await checkPrefSwitch(false, false);
});
