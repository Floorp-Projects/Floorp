/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "CloudStorage",
  "resource://gre/modules/CloudStorage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

const DROPBOX_DOWNLOAD_FOLDER = "Dropbox";
const CLOUD_SERVICES_PREF = "cloud.services.";

function create_subdir(dir, subdirname) {
  let subdir = dir.clone();
  subdir.append(subdirname);
  if (subdir.exists()) {
    subdir.remove(true);
  }
  subdir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return subdir;
}

/**
 * Replaces a directory service entry with a given nsIFile.
 */
function registerFakePath(key, folderName) {
  let dirsvc = Services.dirsvc.QueryInterface(Ci.nsIProperties);

  // Create a directory inside the profile and register it as key
  let profD = dirsvc.get("ProfD", Ci.nsIFile);
  // create a subdir just to keep our files out of the way
  let file = create_subdir(profD, folderName);

  let originalFile;
  try {
    // If a file is already provided save it and undefine, otherwise set will
    // throw for persistent entries (ones that are cached).
    originalFile = dirsvc.get(key, Ci.nsIFile);
    dirsvc.undefine(key);
  } catch (e) {
    // dirsvc.get will throw if nothing provides for the key and dirsvc.undefine
    // will throw if it's not a persistent entry, in either case we don't want
    // to set the original file in cleanup.
    originalFile = undefined;
  }

  dirsvc.set(key, file);
  registerCleanupFunction(() => {
    dirsvc.undefine(key);
    if (originalFile) {
      dirsvc.set(key, originalFile);
    }
  });
}

async function mock_dropbox() {
  // Mock Dropbox Download folder in Home directory
  let downloadFolder = FileUtils.getFile("Home", [
    DROPBOX_DOWNLOAD_FOLDER,
    "Downloads",
  ]);
  if (!downloadFolder.exists()) {
    downloadFolder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  console.log(downloadFolder.path);

  registerCleanupFunction(() => {
    if (downloadFolder.exists()) {
      downloadFolder.remove(true);
    }
  });
}

add_task(async function setup() {
  // Create mock Dropbox download folder for cloudstorage API
  // Set prefs required to display second radio option
  // 'Save to Dropbox' under Downloads
  let folderName = "CloudStorage";
  registerFakePath("Home", folderName);
  await mock_dropbox();
  await SpecialPowers.pushPrefEnv({
    set: [
      [CLOUD_SERVICES_PREF + "api.enabled", true],
      [CLOUD_SERVICES_PREF + "storage.key", "Dropbox"],
    ],
  });
});

add_task(async function test_initProvider() {
  // Get preferred provider key
  let preferredProvider = await CloudStorage.getPreferredProvider();
  is(preferredProvider, "Dropbox", "Cloud Storage preferred provider key");

  let isInitialized = await CloudStorage.init();
  is(isInitialized, true, "Providers Metadata successfully initialized");

  // Get preferred provider in use display name
  let providerDisplayName = await CloudStorage.getProviderIfInUse();
  is(
    providerDisplayName,
    "Dropbox",
    "Cloud Storage preferred provider display name"
  );
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let saveWhereOptions = doc.getElementById("saveWhere");
  let saveToCloud = doc.getElementById("saveToCloud");
  is(saveWhereOptions.itemCount, 3, "Radio options count");
  is_element_visible(saveToCloud, "Save to Dropbox option is visible");

  let saveTo = doc.getElementById("saveTo");
  ok(saveTo.selected, "Ensure first option is selected by default");
  is(
    Services.prefs.getIntPref("browser.download.folderList"),
    1,
    "Set to system downloadsfolder as the default download location"
  );

  let downloadFolder = doc.getElementById("downloadFolder");
  let chooseFolder = doc.getElementById("chooseFolder");
  is(downloadFolder.disabled, false, "downloadFolder filefield is enabled");
  is(chooseFolder.disabled, false, "chooseFolder button is enabled");

  // Test click of second radio option sets browser.download.folderList as 3
  // which means the default download location is elsewhere as specified by
  // cloud storage API getDownloadFolder and pref cloud.services.storage.key
  saveToCloud.click();
  is(
    Services.prefs.getIntPref("browser.download.folderList"),
    3,
    "Default download location is elsewhere as specified by cloud storage API"
  );
  is(downloadFolder.disabled, true, "downloadFolder filefield is disabled");
  is(chooseFolder.disabled, true, "chooseFolder button is disabled");

  // Test selecting first radio option enables downloadFolder filefield and chooseFolder button
  saveTo.click();
  is(downloadFolder.disabled, false, "downloadFolder filefield is enabled");
  is(chooseFolder.disabled, false, "chooseFolder button is enabled");

  // Test selecting third radio option keeps downloadFolder and chooseFolder elements disabled
  let alwaysAsk = doc.getElementById("alwaysAsk");
  saveToCloud.click();
  alwaysAsk.click();
  is(downloadFolder.disabled, true, "downloadFolder filefield is disabled");
  is(chooseFolder.disabled, true, "chooseFolder button is disabled");
  saveTo.click();
  ok(saveTo.selected, "Reset back first option as selected by default");

  gBrowser.removeCurrentTab();
});
