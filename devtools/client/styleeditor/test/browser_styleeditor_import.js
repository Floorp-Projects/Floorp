/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the import button in the UI works.

// http rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

var tempScope = {};
ChromeUtils.import("resource://gre/modules/FileUtils.jsm", tempScope);
var FileUtils = tempScope.FileUtils;

const FILENAME = "styleeditor-import-test.css";
const SOURCE = "body{background:red;}";

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const added = ui.once("test:editor-updated");
  importSheet(ui, panel.panelWindow);

  info("Waiting for editor to be added for the imported sheet.");
  const editor = await added;

  is(editor.savedFile.leafName, FILENAME,
     "imported stylesheet will be saved directly into the same file");
  is(editor.friendlyName, FILENAME,
     "imported stylesheet has the same name as the filename");
});

function importSheet(ui, panelWindow) {
  // create file to import first
  const file = FileUtils.getFile("ProfD", [FILENAME]);
  const ostream = FileUtils.openSafeFileOutputStream(file);
  const converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  const istream = converter.convertToInputStream(SOURCE);
  NetUtil.asyncCopy(istream, ostream, function() {
    FileUtils.closeSafeFileOutputStream(ostream);

    // click the import button now that the file to import is ready
    ui._mockImportFile = file;

    waitForFocus(function() {
      const document = panelWindow.document;
      const importButton = document.querySelector(".style-editor-importButton");
      ok(importButton, "import button exists");

      EventUtils.synthesizeMouseAtCenter(importButton, {}, panelWindow);
    }, panelWindow);
  });
}
