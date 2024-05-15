/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

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
    noMstoneChange: true,
  },
  {
    description: "mstone changed and no update",
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

add_task(async function test_bug538331() {
  // Reset the startup page pref since it may have been set by other tests
  // and we will assume it is (non-test) default.
  await SpecialPowers.pushPrefEnv({
    clear: [["browser.startup.page"]],
  });

  let originalMstone = Services.prefs.getCharPref(PREF_MSTONE);

  // Set the preferences needed for the test: they will be cleared up
  // after it runs.
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MSTONE, originalMstone],
      [PREF_OVERRIDE_URL, DEFAULT_PREF_URL],
    ],
  });

  registerCleanupFunction(async () => {
    let activeUpdateFile = getActiveUpdateFile();
    activeUpdateFile.remove(false);
    reloadUpdateManagerData(true);
  });

  // Clear any pre-existing override in defaultArgs that are hanging around.
  // This will also set the browser.startup.homepage_override.mstone preference
  // if it isn't already set.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

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
      writeFile(XML_PREFIX + actionsXML + XML_SUFFIX, getActiveUpdateFile());
    } else {
      writeFile(XML_EMPTY, getActiveUpdateFile());
    }
    writeSuccessUpdateStatusFile();

    reloadUpdateManagerData(false);

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
  }
});

/**
 * Removes the updates.xml file and returns the nsIFile for the
 * active-update.xml file.
 *
 * @return  The nsIFile for the active-update.xml file.
 */
function getActiveUpdateFile() {
  let updateRootDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  let updatesFile = updateRootDir.clone();
  updatesFile.append("updates.xml");
  if (updatesFile.exists()) {
    // The following is non-fatal.
    try {
      updatesFile.remove(false);
    } catch (e) {}
  }
  let activeUpdateFile = updateRootDir.clone();
  activeUpdateFile.append("active-update.xml");
  return activeUpdateFile;
}

/**
 * Reloads the update xml files.
 *
 * @param  skipFiles (optional)
 *         If true, the update xml files will not be read and the metadata will
 *         be reset. If false (the default), the update xml files will be read
 *         to populate the update metadata.
 */
function reloadUpdateManagerData(skipFiles = false) {
  Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .internal.reload(skipFiles);
}

/**
 * Writes the specified text to the specified file.
 *
 * @param {string} aText
 *   The string to write to the file.
 * @param {nsIFile} aFile
 *   The file to write to.
 */
function writeFile(aText, aFile) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;

  if (!aFile.exists()) {
    aFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  fos.init(aFile, flags, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}

/**
 * If we want the update system to treat the update we wrote out as one that it
 * just installed, we need to make it think that the update state is
 * "succeeded".
 */
function writeSuccessUpdateStatusFile() {
  const statusFile = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  statusFile.append("updates");
  statusFile.append("0");
  statusFile.append("update.status");
  writeFile("succeeded", statusFile);
}
