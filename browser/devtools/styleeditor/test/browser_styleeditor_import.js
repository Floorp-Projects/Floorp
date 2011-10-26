/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// http rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

Components.utils.import("resource://gre/modules/FileUtils.jsm");
const FILENAME = "styleeditor-import-test.css";
const SOURCE = "body{background:red;}";


function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    aChrome.addChromeListener({
      onContentAttach: run,
      onEditorAdded: testEditorAdded
    });
    if (aChrome.isContentAttached) {
      run(aChrome);
    }
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  is(aChrome.editors.length, 2,
     "there is 2 stylesheets initially");
}

function testImport(aChrome, aEditor)
{
  // create file to import first
  let file = FileUtils.getFile("ProfD", [FILENAME]);
  let ostream = FileUtils.openSafeFileOutputStream(file);
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let istream = converter.convertToInputStream(SOURCE);
  NetUtil.asyncCopy(istream, ostream, function (status) {
    FileUtils.closeSafeFileOutputStream(ostream);

    // click the import button now that the file to import is ready
    aChrome._mockImportFile = file;

    waitForFocus(function () {
      let document = gChromeWindow.document
      let importButton = document.querySelector(".style-editor-importButton");
      EventUtils.synthesizeMouseAtCenter(importButton, {}, gChromeWindow);
    }, gChromeWindow);
  });
}

let gAddedCount = 0;
function testEditorAdded(aChrome, aEditor)
{
  if (++gAddedCount == 2) {
    // test import after the 2 initial stylesheets have been loaded
    if (!aChrome.editors[0].sourceEditor) {
      aChrome.editors[0].addActionListener({
        onAttach: function () {
          testImport(aChrome);
        }
      });
    } else {
      testImport(aChrome);
    }
  }

  if (!aEditor.hasFlag("imported")) {
    return;
  }

  ok(!aEditor.hasFlag("inline"),
     "imported stylesheet does not have INLINE flag");
  ok(aEditor.savedFile,
     "imported stylesheet will be saved directly into the same file");
  is(aEditor.getFriendlyName(), FILENAME,
     "imported stylesheet has the same name as the filename");

  finish();
}
