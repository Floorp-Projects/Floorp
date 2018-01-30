"use strict";

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource:///modules/ChromeMigrationUtils.jsm");

// Setup chrome user data path for all platforms.
ChromeMigrationUtils.getChromeUserDataPath = () => {
  return do_get_file("Library/Application Support/Google/Chrome/").path;
};

add_task(async function test_getExtensionList_function() {
  let extensionList = await ChromeMigrationUtils.getExtensionList("Default");
  Assert.equal(extensionList.length, 2, "There should be 2 extensions installed.");
  Assert.deepEqual(extensionList.find(extension => extension.id == "fake-extension-1"), {
    id: "fake-extension-1",
    name: "Fake Extension 1",
    description: "It is the description of fake extension 1.",
  }, "First extension should match expectations.");
  Assert.deepEqual(extensionList.find(extension => extension.id == "fake-extension-2"), {
    id: "fake-extension-2",
    name: "Fake Extension 2",
    description: "It is the description of fake extension 2.",
  }, "Second extension should match expectations.");
});

add_task(async function test_getExtensionInformation_function() {
  let extension = await ChromeMigrationUtils.getExtensionInformation("fake-extension-1", "Default");
  Assert.deepEqual(extension, {
    id: "fake-extension-1",
    name: "Fake Extension 1",
    description: "It is the description of fake extension 1.",
  }, "Should get the extension information correctly.");
});

add_task(async function test_getLocaleString_function() {
  let name = await ChromeMigrationUtils._getLocaleString("__MSG_name__", "en_US", "fake-extension-1", "Default");
  Assert.deepEqual(name, "Fake Extension 1", "The value of __MSG_name__ locale key is Fake Extension 1.");
});

add_task(async function test_isExtensionInstalled_function() {
  let isInstalled = await ChromeMigrationUtils.isExtensionInstalled("fake-extension-1", "Default");
  Assert.ok(isInstalled, "The fake-extension-1 extension should be installed.");
});

add_task(async function test_getLastUsedProfileId_function() {
  let profileId = await ChromeMigrationUtils.getLastUsedProfileId();
  Assert.equal(profileId, "Default", "The last used profile ID should be Default.");
});
