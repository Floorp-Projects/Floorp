"use strict";

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(function* () {
  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  let container = document.querySelector(`[data-addon-id="${ADDON_ID}"]`);
  let filePath = container.querySelector(".file-path");
  let expectedFilePath = "browser/devtools/client/aboutdebugging/test/addons/unpacked/";

  // Verify that the path to the install location is shown next to its label.
  ok(filePath, "file path is in DOM");
  ok(filePath.textContent.endsWith(expectedFilePath), "file path is set correctly");
  is(filePath.previousElementSibling.textContent, "Location", "file path has label");

  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  yield closeAboutDebugging(tab);
});
