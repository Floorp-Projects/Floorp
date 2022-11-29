"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { ChromeMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/ChromeMigrationUtils.sys.mjs"
);

function getRootPath() {
  let dirKey;
  if (AppConstants.platform == "win") {
    dirKey = "LocalAppData";
  } else if (AppConstants.platform == "macosx") {
    dirKey = "ULibDir";
  } else {
    dirKey = "Home";
  }
  return Services.dirsvc.get(dirKey, Ci.nsIFile).path;
}

add_task(async function test_getDataPath_function() {
  let chromeUserDataPath = ChromeMigrationUtils.getDataPath("Chrome");
  let chromiumUserDataPath = ChromeMigrationUtils.getDataPath("Chromium");
  let canaryUserDataPath = ChromeMigrationUtils.getDataPath("Canary");
  if (AppConstants.platform == "win") {
    Assert.equal(
      chromeUserDataPath,
      PathUtils.join(getRootPath(), "Google", "Chrome", "User Data"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      PathUtils.join(getRootPath(), "Chromium", "User Data"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(
      canaryUserDataPath,
      PathUtils.join(getRootPath(), "Google", "Chrome SxS", "User Data"),
      "Should get the path of Canary data directory."
    );
  } else if (AppConstants.platform == "macosx") {
    Assert.equal(
      chromeUserDataPath,
      PathUtils.join(getRootPath(), "Application Support", "Google", "Chrome"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      PathUtils.join(getRootPath(), "Application Support", "Chromium"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(
      canaryUserDataPath,
      PathUtils.join(
        getRootPath(),
        "Application Support",
        "Google",
        "Chrome Canary"
      ),
      "Should get the path of Canary data directory."
    );
  } else {
    Assert.equal(
      chromeUserDataPath,
      PathUtils.join(getRootPath(), ".config", "google-chrome"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      PathUtils.join(getRootPath(), ".config", "chromium"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(canaryUserDataPath, null, "Should get null for Canary.");
  }
});

add_task(async function test_getExtensionPath_function() {
  let extensionPath = ChromeMigrationUtils.getExtensionPath("Default");
  let expectedPath;
  if (AppConstants.platform == "win") {
    expectedPath = PathUtils.join(
      getRootPath(),
      "Google",
      "Chrome",
      "User Data",
      "Default",
      "Extensions"
    );
  } else if (AppConstants.platform == "macosx") {
    expectedPath = PathUtils.join(
      getRootPath(),
      "Application Support",
      "Google",
      "Chrome",
      "Default",
      "Extensions"
    );
  } else {
    expectedPath = PathUtils.join(
      getRootPath(),
      ".config",
      "google-chrome",
      "Default",
      "Extensions"
    );
  }
  Assert.equal(
    extensionPath,
    expectedPath,
    "Should get the path of extensions directory."
  );
});
