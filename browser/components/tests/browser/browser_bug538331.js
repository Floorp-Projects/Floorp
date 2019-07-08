/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PREF_POSTUPDATE = "app.update.postupdate";
const PREF_MSTONE = "browser.startup.homepage_override.mstone";
const PREF_OVERRIDE_URL = "startup.homepage_override_url";

const DEFAULT_PREF_URL = "http://pref.example.com/";
const DEFAULT_UPDATE_URL = "http://example.com/";

const XML_EMPTY =
  '<?xml version="1.0"?><updates xmlns=' +
  '"http://www.mozilla.org/2005/app-update"></updates>';

const XML_PREFIX =
  '<updates xmlns="http://www.mozilla.org/2005/app-update"' +
  '><update appVersion="1.0" buildID="20080811053724" ' +
  'channel="nightly" displayVersion="Version 1.0" ' +
  'installDate="1238441400314" isCompleteUpdate="true" ' +
  'name="Update Test 1.0" type="minor" detailsURL=' +
  '"http://example.com/" previousAppVersion="1.0" ' +
  'serviceURL="https://example.com/" ' +
  'statusText="The Update was successfully installed" ' +
  'foregroundDownload="true"';

const XML_SUFFIX =
  '><patch type="complete" URL="http://example.com/" ' +
  'size="775" selected="true" state="succeeded"/>' +
  "</update></updates>";

// nsBrowserContentHandler.js defaultArgs tests
const BCH_TESTS = [
  {
    description: "no mstone change and no update",
    noPostUpdatePref: true,
    noMstoneChange: true,
  },
  {
    description: "mstone changed and no update",
    noPostUpdatePref: true,
    prefURL: DEFAULT_PREF_URL,
  },
  {
    description: "no mstone change and update with 'showURL' for actions",
    actions: "showURL",
    noMstoneChange: true,
  },
  {
    description: "update without actions",
    prefURL: DEFAULT_PREF_URL,
  },
  {
    description: "update with 'showURL' for actions",
    actions: "showURL",
    prefURL: DEFAULT_PREF_URL,
  },
  {
    description: "update with 'showURL' for actions and openURL",
    actions: "showURL",
    openURL: DEFAULT_UPDATE_URL,
  },
  {
    description: "update with 'extra showURL' for actions",
    actions: "extra showURL",
    prefURL: DEFAULT_PREF_URL,
  },
  {
    description: "update with 'extra showURL' for actions and openURL",
    actions: "extra showURL",
    openURL: DEFAULT_UPDATE_URL,
  },
  {
    description: "update with 'silent' for actions",
    actions: "silent",
  },
  {
    description: "update with 'silent showURL extra' for actions and openURL",
    actions: "silent showURL extra",
  },
];

var gOriginalMStone;
var gOriginalOverrideURL;

function test() {
  waitForExplicitFinish();

  // Reset the startup page pref since it may have been set by other tests
  // and we will assume it is default.
  Services.prefs.clearUserPref("browser.startup.page");

  if (Services.prefs.prefHasUserValue(PREF_MSTONE)) {
    gOriginalMStone = Services.prefs.getCharPref(PREF_MSTONE);
  }

  if (Services.prefs.prefHasUserValue(PREF_OVERRIDE_URL)) {
    gOriginalOverrideURL = Services.prefs.getCharPref(PREF_OVERRIDE_URL);
  }

  testDefaultArgs();
}

function finish_test() {
  // Reset browser.startup.homepage_override.mstone to the original value or
  // clear it if it didn't exist.
  if (gOriginalMStone) {
    Services.prefs.setCharPref(PREF_MSTONE, gOriginalMStone);
  } else if (Services.prefs.prefHasUserValue(PREF_MSTONE)) {
    Services.prefs.clearUserPref(PREF_MSTONE);
  }

  // Reset startup.homepage_override_url to the original value or clear it if
  // it didn't exist.
  if (gOriginalOverrideURL) {
    Services.prefs.setCharPref(PREF_OVERRIDE_URL, gOriginalOverrideURL);
  } else if (Services.prefs.prefHasUserValue(PREF_OVERRIDE_URL)) {
    Services.prefs.clearUserPref(PREF_OVERRIDE_URL);
  }

  writeUpdatesToXMLFile(XML_EMPTY);
  reloadUpdateManagerData();

  finish();
}

// Test the defaultArgs returned by nsBrowserContentHandler after an update
function testDefaultArgs() {
  // Clear any pre-existing override in defaultArgs that are hanging around.
  // This will also set the browser.startup.homepage_override.mstone preference
  // if it isn't already set.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  let originalMstone = Services.prefs.getCharPref(PREF_MSTONE);

  Services.prefs.setCharPref(PREF_OVERRIDE_URL, DEFAULT_PREF_URL);

  writeUpdatesToXMLFile(XML_EMPTY);
  reloadUpdateManagerData();

  for (let i = 0; i < BCH_TESTS.length; i++) {
    let testCase = BCH_TESTS[i];
    ok(
      true,
      "Test nsBrowserContentHandler " + (i + 1) + ": " + testCase.description
    );

    if (testCase.actions) {
      let actionsXML = ' actions="' + testCase.actions + '"';
      if (testCase.openURL) {
        actionsXML += ' openURL="' + testCase.openURL + '"';
      }
      writeUpdatesToXMLFile(XML_PREFIX + actionsXML + XML_SUFFIX);
    } else {
      writeUpdatesToXMLFile(XML_EMPTY);
    }

    reloadUpdateManagerData();

    let noOverrideArgs = Cc["@mozilla.org/browser/clh;1"].getService(
      Ci.nsIBrowserHandler
    ).defaultArgs;

    let overrideArgs = "";
    if (testCase.prefURL) {
      overrideArgs = testCase.prefURL;
    } else if (testCase.openURL) {
      overrideArgs = testCase.openURL;
    }

    if (overrideArgs == "" && noOverrideArgs) {
      overrideArgs = noOverrideArgs;
    } else if (noOverrideArgs) {
      overrideArgs += "|" + noOverrideArgs;
    }

    if (testCase.noMstoneChange === undefined) {
      Services.prefs.setCharPref(PREF_MSTONE, "PreviousMilestone");
    }

    if (testCase.noPostUpdatePref == undefined) {
      Services.prefs.setBoolPref(PREF_POSTUPDATE, true);
    }

    let defaultArgs = Cc["@mozilla.org/browser/clh;1"].getService(
      Ci.nsIBrowserHandler
    ).defaultArgs;
    is(defaultArgs, overrideArgs, "correct value returned by defaultArgs");

    if (testCase.noMstoneChange === undefined || !testCase.noMstoneChange) {
      let newMstone = Services.prefs.getCharPref(PREF_MSTONE);
      is(
        originalMstone,
        newMstone,
        "preference " + PREF_MSTONE + " should have been updated"
      );
    }

    if (Services.prefs.prefHasUserValue(PREF_POSTUPDATE)) {
      Services.prefs.clearUserPref(PREF_POSTUPDATE);
    }
  }

  finish_test();
}

/* Reloads the update metadata from disk */
function reloadUpdateManagerData() {
  Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "um-reload-update-data", "");
}

function writeUpdatesToXMLFile(aText) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;

  let file = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  file.append("updates.xml");
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  if (!file.exists()) {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}
