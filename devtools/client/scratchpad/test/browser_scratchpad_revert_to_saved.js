/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 751744 */

// Reference to the Scratchpad object.
var gScratchpad;

// Reference to the temporary nsIFiles.
var gFile;

// Temporary file name.
var gFileName = "testFileForBug751744.tmp";

// Content for the temporary file.
var gFileContent = "/* this file is already saved */\n" +
                   "function foo() { alert('bar') }";

// Reference to the menu entry.
var menu;

function startTest() {
  gScratchpad = gScratchpadWindow.Scratchpad;
  menu = gScratchpadWindow.document.getElementById("sp-cmd-revert");
  createAndLoadTemporaryFile();
}

function testAfterSaved() {
  // Check if the revert menu is disabled as the file is at saved state.
  ok(menu.hasAttribute("disabled"), "The revert menu entry is disabled.");

  // chancging the text in the file
  gScratchpad.setText(gScratchpad.getText() + "\nfoo();");
  // Checking the text got changed
  is(gScratchpad.getText(), gFileContent + "\nfoo();",
     "The text changed the first time.");

  // Revert menu now should be enabled.
  ok(!menu.hasAttribute("disabled"),
     "The revert menu entry is enabled after changing text first time");

  // reverting back to last saved state.
  gScratchpad.revertFile(testAfterRevert);
}

function testAfterRevert() {
  // Check if the file's text got reverted
  is(gScratchpad.getText(), gFileContent,
     "The text reverted back to original text.");
  // The revert menu should be disabled again.
  ok(menu.hasAttribute("disabled"),
     "The revert menu entry is disabled after reverting.");

  // chancging the text in the file again
  gScratchpad.setText(gScratchpad.getText() + "\nalert(foo.toSource());");
  // Saving the file.
  gScratchpad.saveFile(testAfterSecondSave);
}

function testAfterSecondSave() {
  // revert menu entry should be disabled.
  ok(menu.hasAttribute("disabled"),
     "The revert menu entry is disabled after saving.");

  // changing the text.
  gScratchpad.setText(gScratchpad.getText() + "\nfoo();");

  // revert menu entry should get enabled yet again.
  ok(!menu.hasAttribute("disabled"),
     "The revert menu entry is enabled after changing text third time");

  // reverting back to last saved state.
  gScratchpad.revertFile(testAfterSecondRevert);
}

function testAfterSecondRevert() {
  // Check if the file's text got reverted
  is(gScratchpad.getText(), gFileContent + "\nalert(foo.toSource());",
     "The text reverted back to the changed saved text.");
  // The revert menu should be disabled again.
  ok(menu.hasAttribute("disabled"),
     "Revert menu entry is disabled after reverting to changed saved state.");
  gFile.remove(false);
  gFile = gScratchpad = menu = null;
  finish();
}

function createAndLoadTemporaryFile() {
  // Create a temporary file.
  gFile = FileUtils.getFile("TmpD", [gFileName]);
  gFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  // Write the temporary file.
  const fout = Cc["@mozilla.org/network/file-output-stream;1"]
             .createInstance(Ci.nsIFileOutputStream);
  fout.init(gFile.QueryInterface(Ci.nsIFile), 0x02 | 0x08 | 0x20,
            0o644, fout.DEFER_OPEN);

  const converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                  .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  const fileContentStream = converter.convertToInputStream(gFileContent);

  NetUtil.asyncCopy(fileContentStream, fout, tempFileSaved);
}

function tempFileSaved(aStatus) {
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was saved successfully");

  // Import the file into Scratchpad.
  gScratchpad.setFilename(gFile.path);
  gScratchpad.importFromFile(gFile.QueryInterface(Ci.nsIFile), true,
                             testAfterSaved);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(startTest);
  });

  gBrowser.loadURI("data:text/html,<p>test reverting to last saved state of" +
                   " a file </p>");
}
