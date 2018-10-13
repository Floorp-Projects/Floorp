ChromeUtils.import("resource://gre/modules/Services.jsm");

const SUBDIALOG_URL = "chrome://browser/content/preferences/connection.xul";
const TRR_MODE_PREF = "network.trr.mode";
const TRR_URI_PREF = "network.trr.uri";

const modeCheckboxSelector = "#networkDnsOverHttps";
const uriTextboxSelector = "#networkDnsOverHttpsUrl";
const defaultPrefValues = Object.freeze({
  [TRR_MODE_PREF]: 0,
  [TRR_URI_PREF]: "https://mozilla.cloudflare-dns.com/dns-query",
});

function resetPrefs() {
  Services.prefs.clearUserPref(TRR_MODE_PREF);
  Services.prefs.clearUserPref(TRR_URI_PREF);
}

async function setup() {
  await new Promise(res => {
    resetPrefs();
    open_preferences(res);
  });
}

async function openConnectionsSubDialog() {
  /*
    The connection dialog has type="child", So it has to be opened as a sub dialog
    of the main pref tab. Prefs only get updated after the subdialog is confirmed & closed
  */
  let dialog = await openAndLoadSubDialog(SUBDIALOG_URL);
  ok(dialog, "connection window opened");
  return dialog;
}

function waitForPrefObserver(name) {
  return new Promise(resolve => {
    const observer = {
      observe(aSubject, aTopic, aData) {
        if (aData == name) {
          Services.prefs.removeObserver(name, observer);
          resolve();
        }
      },
    };
    Services.prefs.addObserver(name, observer);
  });
}

async function testWithProperties(props) {
  info("testing with " + JSON.stringify(props));
  if (props.hasOwnProperty(TRR_MODE_PREF)) {
    Services.prefs.setIntPref(TRR_MODE_PREF, props[TRR_MODE_PREF]);
  }
  if (props.hasOwnProperty(TRR_URI_PREF)) {
    Services.prefs.setStringPref(TRR_URI_PREF, props[TRR_URI_PREF]);
  }

  let dialog = await openConnectionsSubDialog();
  let doc = dialog.document;
  let win = doc.ownerGlobal;
  let dialogClosingPromise = BrowserTestUtils.waitForEvent(doc.documentElement,
                                                           "dialogclosing");
  let modeCheckbox = doc.querySelector(modeCheckboxSelector);
  let uriTextbox = doc.querySelector(uriTextboxSelector);
  let uriPrefChangedPromise;
  let modePrefChangedPromise;

  if (props.hasOwnProperty("expectedModeChecked")) {
    is(modeCheckbox.checked, props.expectedModeChecked, "mode checkbox has expected checked state");
  }
  if (props.hasOwnProperty("expectedUriValue")) {
    is(uriTextbox.value, props.expectedUriValue, "URI textbox has expected value");
  }
  if (props.clickMode) {
    modePrefChangedPromise = waitForPrefObserver(TRR_MODE_PREF);
    modeCheckbox.scrollIntoView();
    EventUtils.synthesizeMouseAtCenter(modeCheckbox, {}, win);
  }
  if (props.hasOwnProperty("inputUriKeys")) {
    uriPrefChangedPromise = waitForPrefObserver(TRR_URI_PREF);
    uriTextbox.focus();
    uriTextbox.value = props.inputUriKeys;
    uriTextbox.dispatchEvent(new win.Event("input", {bubbles: true}));
    uriTextbox.dispatchEvent(new win.Event("change", {bubbles: true}));
  }

  doc.documentElement.acceptDialog();

  let dialogClosingEvent = await dialogClosingPromise;
  ok(dialogClosingEvent, "connection window closed");

  await Promise.all([uriPrefChangedPromise, modePrefChangedPromise]);

  if (props.hasOwnProperty("expectedFinalUriPref")) {
    let uriPref = Services.prefs.getStringPref(TRR_URI_PREF);
    is(uriPref, props.expectedFinalUriPref, "uri pref ended up with the expected value");
  }

  if (props.hasOwnProperty("expectedModePref")) {
    let modePref = Services.prefs.getIntPref(TRR_MODE_PREF);
    is(modePref, props.expectedModePref, "mode pref ended up with the expected value");
  }
}

registerCleanupFunction(resetPrefs);

add_task(async function default_values() {
  let uriPref = Services.prefs.getStringPref(TRR_URI_PREF);
  let modePref = Services.prefs.getIntPref(TRR_MODE_PREF);
  is(modePref, defaultPrefValues[TRR_MODE_PREF],
     `Actual value of ${TRR_MODE_PREF} matches expected default value`);
  is(uriPref, defaultPrefValues[TRR_URI_PREF],
     `Actual value of ${TRR_MODE_PREF} matches expected default value`);
});

let testVariations = [
  // verify state with defaults
  { expectedModePref: 0, expectedUriValue: "https://mozilla.cloudflare-dns.com/dns-query" },

  // verify each of the modes maps to the correct checked state
  { [TRR_MODE_PREF]: 0, expectedModeChecked: false },
  { [TRR_MODE_PREF]: 1, expectedModeChecked: true },
  { [TRR_MODE_PREF]: 2, expectedModeChecked: true },
  { [TRR_MODE_PREF]: 3, expectedModeChecked: true },
  { [TRR_MODE_PREF]: 4, expectedModeChecked: true },
  { [TRR_MODE_PREF]: 5, expectedModeChecked: false },
  // verify an out of bounds mode value maps to the correct checked state
  { [TRR_MODE_PREF]: 77, expectedModeChecked: false },

  // verify toggling the checkbox gives the right outcomes
  { clickMode: true, expectedModeValue: 2, expectedUriValue: "https://mozilla.cloudflare-dns.com/dns-query" },
  {
    [TRR_MODE_PREF]: 4,
    expectedModeChecked: true, clickMode: true, expectedModePref: 0,
  },
  {
    [TRR_MODE_PREF]: 2, [TRR_URI_PREF]: "https://example.com",
    expectedModeValue: true, expectedUriValue: "https://example.com",
  },
  {
    clickMode: true, inputUriKeys: "https://example.com",
    expectedModePref: 2, expectedFinalUriPref: "https://example.com",
  },

  // verify the uri can be cleared
  {
    [TRR_MODE_PREF]: 2, [TRR_URI_PREF]: "https://example.com",
    expectedUriValue: "https://example.com", inputUriKeys: "", expectedFinalUriPref: "",
  },

  // verify uri gets sanitized
  {
    clickMode: true, inputUriKeys: "  https://example.com ",
    expectedModePref: 2, expectedFinalUriPref: "https://example.com",
  },
];

for (let props of testVariations) {
  add_task(async function() {
    await setup();
    await testWithProperties(props);
    resetPrefs();
    gBrowser.removeCurrentTab();
  });
}
