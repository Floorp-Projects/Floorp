"use strict";

const { ChromeMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/ChromeMigrationUtils.sys.mjs"
);

const SUB_DIRECTORIES = {
  win: {
    Chrome: ["Google", "Chrome", "User Data"],
    Chromium: ["Chromium", "User Data"],
    Canary: ["Google", "Chrome SxS", "User Data"],
  },
  macosx: {
    Chrome: ["Application Support", "Google", "Chrome"],
    Chromium: ["Application Support", "Chromium"],
    Canary: ["Application Support", "Google", "Chrome Canary"],
  },
  linux: {
    Chrome: [".config", "google-chrome"],
    Chromium: [".config", "chromium"],
    Canary: [],
  },
};

add_task(async function setup_fakePaths() {
  let pathId;
  if (AppConstants.platform == "macosx") {
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    pathId = "LocalAppData";
  } else {
    pathId = "Home";
  }

  registerFakePath(pathId, do_get_file("chromefiles/", true));
});

add_task(async function test_getDataPath_function() {
  let projects = ["Chrome", "Chromium", "Canary"];
  let rootPath = getRootPath();

  for (let project of projects) {
    let subfolders = SUB_DIRECTORIES[AppConstants.platform][project];

    await IOUtils.makeDirectory(PathUtils.join(rootPath, ...subfolders), {
      createAncestor: true,
      ignoreExisting: true,
    });
  }

  let chromeUserDataPath = await ChromeMigrationUtils.getDataPath("Chrome");
  let chromiumUserDataPath = await ChromeMigrationUtils.getDataPath("Chromium");
  let canaryUserDataPath = await ChromeMigrationUtils.getDataPath("Canary");
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
  let extensionPath = await ChromeMigrationUtils.getExtensionPath("Default");
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
