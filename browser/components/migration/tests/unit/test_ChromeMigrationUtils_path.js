"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { ChromeMigrationUtils } = ChromeUtils.import(
  "resource:///modules/ChromeMigrationUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function getRootPath() {
  let dirKey;
  if (AppConstants.platform == "win") {
    dirKey = "winLocalAppDataDir";
  } else if (AppConstants.platform == "macosx") {
    dirKey = "macUserLibDir";
  } else {
    dirKey = "homeDir";
  }
  return OS.Constants.Path[dirKey];
}

add_task(async function test_getDataPath_function() {
  let chromeUserDataPath = ChromeMigrationUtils.getDataPath("Chrome");
  let chromiumUserDataPath = ChromeMigrationUtils.getDataPath("Chromium");
  let canaryUserDataPath = ChromeMigrationUtils.getDataPath("Canary");
  if (AppConstants.platform == "win") {
    Assert.equal(
      chromeUserDataPath,
      OS.Path.join(getRootPath(), "Google", "Chrome", "User Data"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      OS.Path.join(getRootPath(), "Chromium", "User Data"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(
      canaryUserDataPath,
      OS.Path.join(getRootPath(), "Google", "Chrome SxS", "User Data"),
      "Should get the path of Canary data directory."
    );
  } else if (AppConstants.platform == "macosx") {
    Assert.equal(
      chromeUserDataPath,
      OS.Path.join(getRootPath(), "Application Support", "Google", "Chrome"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      OS.Path.join(getRootPath(), "Application Support", "Chromium"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(
      canaryUserDataPath,
      OS.Path.join(
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
      OS.Path.join(getRootPath(), ".config", "google-chrome"),
      "Should get the path of Chrome data directory."
    );
    Assert.equal(
      chromiumUserDataPath,
      OS.Path.join(getRootPath(), ".config", "chromium"),
      "Should get the path of Chromium data directory."
    );
    Assert.equal(canaryUserDataPath, null, "Should get null for Canary.");
  }
});

add_task(async function test_getExtensionPath_function() {
  let extensionPath = ChromeMigrationUtils.getExtensionPath("Default");
  let expectedPath;
  if (AppConstants.platform == "win") {
    expectedPath = OS.Path.join(
      getRootPath(),
      "Google",
      "Chrome",
      "User Data",
      "Default",
      "Extensions"
    );
  } else if (AppConstants.platform == "macosx") {
    expectedPath = OS.Path.join(
      getRootPath(),
      "Application Support",
      "Google",
      "Chrome",
      "Default",
      "Extensions"
    );
  } else {
    expectedPath = OS.Path.join(
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
