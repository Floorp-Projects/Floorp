/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test was based on the test browser_bug538331.js

const UPDATE_PROVIDED_PAGE = "https://default.example.com/";
const POLICY_PROVIDED_PAGE = "https://policy.example.com/";

const PREF_MSTONE = "browser.startup.homepage_override.mstone";

/*
 * The important parts for this test are:
 *  - actions="showURL"
 *  - openURL="${UPDATE_PROVIDED_PAGE}"
 */
const XML_UPDATE = `<?xml version="1.0"?>
<updates xmlns="http://www.mozilla.org/2005/app-update">
  <update appVersion="1.0" buildID="20080811053724" channel="nightly"
          displayVersion="Version 1.0" installDate="1238441400314"
          isCompleteUpdate="true" name="Update Test 1.0" type="minor"
          detailsURL="http://example.com/" previousAppVersion="1.0"
          serviceURL="https://example.com/" statusText="The Update was successfully installed"
          foregroundDownload="true"
          actions="showURL"
          openURL="${UPDATE_PROVIDED_PAGE}">
    <patch type="complete" URL="http://example.com/" size="775" selected="true" state="succeeded"/>
  </update>
</updates>`;

add_task(async function test_override_postupdate_page() {
  let originalMstone = Services.prefs.getCharPref(PREF_MSTONE);
  // Set the preferences needed for the test: they will be cleared up
  // after it runs.
  await SpecialPowers.pushPrefEnv({ set: [[PREF_MSTONE, originalMstone]] });

  registerCleanupFunction(async () => {
    let activeUpdateFile = getActiveUpdateFile();
    activeUpdateFile.remove(false);
    reloadUpdateManagerData(true);
  });

  writeUpdatesToXMLFile(XML_UPDATE);
  reloadUpdateManagerData(false);

  is(
    getPostUpdatePage(),
    UPDATE_PROVIDED_PAGE,
    "Post-update page was provided by active-update.xml."
  );

  // Now perform the same action but set the policy to override this page
  await setupPolicyEngineWithJson({
    policies: {
      OverridePostUpdatePage: POLICY_PROVIDED_PAGE,
    },
  });

  is(
    getPostUpdatePage(),
    POLICY_PROVIDED_PAGE,
    "Post-update page was provided by policy."
  );
});

function getPostUpdatePage() {
  Services.prefs.setCharPref(PREF_MSTONE, "PreviousMilestone");
  return Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler)
    .defaultArgs;
}

/**
 * Removes the updates.xml file and returns the nsIFile for the
 * active-update.xml file.
 *
 * @returns {nsIFile}
 *   The nsIFile for the active-update.xml file.
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
 * @param {boolean} [skipFiles]
 *   If true, the update xml files will not be read and the metadata will
 *   be reset. If false (the default), the update xml files will be read
 *   to populate the update metadata.
 */
function reloadUpdateManagerData(skipFiles = false) {
  Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "um-reload-update-data", skipFiles ? "skip-files" : "");
}

/**
 * Writes the updates specified to the active-update.xml file.
 *
 * @param {string} aText
 *   The updates represented as a string to write to the active-update.xml file.
 */
function writeUpdatesToXMLFile(aText) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;

  let activeUpdateFile = getActiveUpdateFile();
  if (!activeUpdateFile.exists()) {
    activeUpdateFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  fos.init(activeUpdateFile, flags, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}
