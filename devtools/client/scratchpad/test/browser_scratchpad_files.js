/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Scratchpad object.
var gScratchpad;

// Reference to the temporary nsIFile we will work with.
var gFile;

// The temporary file content.
var gFileContent = "hello.world('bug636725');";

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function () {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html,<p>test file open and save in Scratchpad");
}

function runTests()
{
  gScratchpad = gScratchpadWindow.Scratchpad;

  createTempFile("fileForBug636725.tmp", gFileContent, function (aStatus, aFile) {
    ok(Components.isSuccessCode(aStatus),
      "The temporary file was saved successfully");

    gFile = aFile;
    gScratchpad.importFromFile(gFile.QueryInterface(Ci.nsIFile), true,
        fileImported);
  });
}

function fileImported(aStatus, aFileContent)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was imported successfully with Scratchpad");

  is(aFileContent, gFileContent,
     "received data is correct");

  is(gScratchpad.getText(), gFileContent,
     "the editor content is correct");

  is(gScratchpad.dirty, false,
     "the editor marks imported file as saved");

  // Save the file after changes.
  gFileContent += "// omg, saved!";
  gScratchpad.editor.setText(gFileContent);

  gScratchpad.exportToFile(gFile.QueryInterface(Ci.nsIFile), true, true,
                          fileExported);
}

function fileExported(aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was exported successfully with Scratchpad");

  let oldContent = gFileContent;

  // Attempt another file save, with confirmation which returns false.
  gFileContent += "// omg, saved twice!";
  gScratchpad.editor.setText(gFileContent);

  let oldConfirm = gScratchpadWindow.confirm;
  let askedConfirmation = false;
  gScratchpadWindow.confirm = function () {
    askedConfirmation = true;
    return false;
  };

  gScratchpad.exportToFile(gFile.QueryInterface(Ci.nsIFile), false, true,
                          fileExported2);

  gScratchpadWindow.confirm = oldConfirm;

  ok(askedConfirmation, "exportToFile() asked for overwrite confirmation");

  gFileContent = oldContent;

  let channel = NetUtil.newChannel({
    uri: NetUtil.newURI(gFile),
    loadUsingSystemPrincipal: true});
  channel.contentType = "application/javascript";

  // Read back the temporary file.
  NetUtil.asyncFetch(channel, fileRead);
}

function fileExported2()
{
  ok(false, "exportToFile() did not cancel file overwrite");
}

function fileRead(aInputStream, aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was read back successfully");

  let updatedContent =
    NetUtil.readInputStreamToString(aInputStream, aInputStream.available());

  is(updatedContent, gFileContent, "file properly updated");

  // Done!
  gFile.remove(false);
  gFile = null;
  gScratchpad = null;
  finish();
}
