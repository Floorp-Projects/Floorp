/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test was based on the test browser_bug538331.js

const UPDATE_PROVIDED_PAGE = "https://default.example.com/";
const POLICY_PROVIDED_PAGE = "https://policy.example.com/";

const PREF_MSTONE = "browser.startup.homepage_override.mstone";
const PREF_POSTUPDATE = "app.update.postupdate";

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

const XML_EMPTY = `<?xml version="1.0"?>
<updates xmlns="http://www.mozilla.org/2005/app-update">
</updates>`;

let gOriginalMStone = null;

add_task(async function test_override_postupdate_page() {
  // Remember this to clean-up afterwards
  if (Services.prefs.prefHasUserValue(PREF_MSTONE)) {
    gOriginalMStone = Services.prefs.getCharPref(PREF_MSTONE);
  }
  Services.prefs.setBoolPref(PREF_POSTUPDATE, true);

  writeUpdatesToXMLFile(XML_UPDATE);
  reloadUpdateManagerData();

  is(getPostUpdatePage(), UPDATE_PROVIDED_PAGE, "Post-update page was provided by update.xml.");

  // Now perform the same action but set the policy to override this page
  await setupPolicyEngineWithJson({
    "policies": {
      "OverridePostUpdatePage": POLICY_PROVIDED_PAGE,
    },
  });

  is(getPostUpdatePage(), POLICY_PROVIDED_PAGE, "Post-update page was provided by policy.");

  // Clean-up
  writeUpdatesToXMLFile(XML_EMPTY);
  if (gOriginalMStone) {
    Services.prefs.setCharPref(PREF_MSTONE, gOriginalMStone);
  }
  Services.prefs.clearUserPref(PREF_POSTUPDATE);
  reloadUpdateManagerData();
});


function getPostUpdatePage() {
  Services.prefs.setCharPref(PREF_MSTONE, "PreviousMilestone");
  return Cc["@mozilla.org/browser/clh;1"]
           .getService(Ci.nsIBrowserHandler)
           .defaultArgs;
}

function reloadUpdateManagerData() {
  // Reloads the update metadata from disk
  Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager).
  QueryInterface(Ci.nsIObserver).observe(null, "um-reload-update-data", "");
}


function writeUpdatesToXMLFile(aText) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY   = 0x02;
  const MODE_CREATE   = 0x08;
  const MODE_TRUNCATE = 0x20;

  let file = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  file.append("updates.xml");
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(Ci.nsIFileOutputStream);
  if (!file.exists()) {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}
