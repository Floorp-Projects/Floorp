/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

// Reference to the Workspace chrome window object.
let gWorkspaceWindow;

// Reference to the Workspace object.
let gWorkspace;

// Reference to the temporary nsIFile we will work with.
let gFile;

// The temporary file content.
let gFileContent = "hello.world('bug636725');";

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    gWorkspaceWindow = Workspace.openWorkspace();
    gWorkspaceWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,<p>test file open and save in Workspace";
}

function runTests()
{
  gWorkspaceWindow.removeEventListener("load", arguments.callee, false);

  gWorkspace = gWorkspaceWindow.Workspace;

  // Create a temporary file.
  gFile = FileUtils.getFile("TmpD", ["fileForBug636725.tmp"]);
  gFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

  // Write the temporary file.
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(gFile.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            0644, fout.DEFER_OPEN);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let fileContentStream = converter.convertToInputStream(gFileContent);

  NetUtil.asyncCopy(fileContentStream, fout, tempFileSaved);
}

function tempFileSaved(aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was saved successfully");

  // Import the file into Workspace.
  gWorkspace.importFromFile(gFile.QueryInterface(Ci.nsILocalFile),  true,
                            fileImported);
}

function fileImported(aStatus, aFileContent)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was imported successfully with Workspace");

  is(aFileContent, gFileContent,
     "received data is correct");

  is(gWorkspace.textbox.value, gFileContent,
     "the textbox.value is correct");

  // Save the file after changes.
  gFileContent += "// omg, saved!";
  gWorkspace.textbox.value = gFileContent;

  gWorkspace.exportToFile(gFile.QueryInterface(Ci.nsILocalFile), true, true,
                          fileExported);
}

function fileExported(aStatus)
{
  ok(Components.isSuccessCode(aStatus),
     "the temporary file was exported successfully with Workspace");

  let oldContent = gFileContent;

  // Attempt another file save, with confirmation which returns false.
  gFileContent += "// omg, saved twice!";
  gWorkspace.textbox.value = gFileContent;

  let oldConfirm = gWorkspaceWindow.confirm;
  let askedConfirmation = false;
  gWorkspaceWindow.confirm = function() {
    askedConfirmation = true;
    return false;
  };

  gWorkspace.exportToFile(gFile.QueryInterface(Ci.nsILocalFile), false, true,
                          fileExported2);

  gWorkspaceWindow.confirm = oldConfirm;

  ok(askedConfirmation, "exportToFile() asked for overwrite confirmation");

  gFileContent = oldContent;

  let channel = NetUtil.newChannel(gFile);
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
    NetUtil.readInputStreamToString(aInputStream, aInputStream.available());;

  is(updatedContent, gFileContent, "file properly updated");

  // Done!
  gFile.remove(false);
  gFile = null;
  gWorkspace = null;
  gWorkspaceWindow.close();
  gWorkspaceWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
