/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ChromeMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/ChromeMigrationUtils.sys.mjs"
);

const SUB_DIRECTORIES = {
  linux: {
    Chromium: [".config", "chromium"],
    SnapChromium: ["snap", "chromium", "common", "chromium"],
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
  let rootPath = getRootPath();
  let chromiumSubFolders = SUB_DIRECTORIES[AppConstants.platform].Chromium;
  // must remove normal chromium path
  await IOUtils.remove(PathUtils.join(rootPath, ...chromiumSubFolders), {
    ignoreAbsent: true,
  });

  let snapChromiumSubFolders =
    SUB_DIRECTORIES[AppConstants.platform].SnapChromium;
  // must create snap chromium path
  await IOUtils.makeDirectory(
    PathUtils.join(rootPath, ...snapChromiumSubFolders),
    {
      createAncestor: true,
      ignoreExisting: true,
    }
  );

  let chromiumUserDataPath = await ChromeMigrationUtils.getDataPath("Chromium");
  Assert.equal(
    chromiumUserDataPath,
    PathUtils.join(getRootPath(), ...snapChromiumSubFolders),
    "Should get the path of Snap Chromium data directory."
  );
});
