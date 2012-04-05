/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
let NetUtil = tempScope.NetUtil;
let FileUtils = tempScope.FileUtils;

// only finish() when correct number of tests are done
const expected = 5;
var count = 0;
function done()
{
  if (++count == expected) {
    cleanup();
    finish();
  }
}

var gFile;

var oldPrompt = Services.prompt;
var promptButton = -1;

function test()
{
  waitForExplicitFinish();

  gFile = createTempFile("fileForBug653427.tmp");
  writeFile(gFile, "text", testUnsaved.call(this));

  Services.prompt = {
    confirmEx: function() {
      return promptButton;
    }
  };

  testNew();
  testSavedFile();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = "data:text/html,<p>test scratchpad save file prompt on closing";
}

function testNew()
{
  openScratchpad(function(win) {
    win.Scratchpad.close(function() {
      ok(win.closed, "new scratchpad window should close without prompting")
      done();
    });
  }, {noFocus: true});
}

function testSavedFile()
{
  openScratchpad(function(win) {
    win.Scratchpad.filename = "test.js";
    win.Scratchpad.editor.dirty = false;
    win.Scratchpad.close(function() {
      ok(win.closed, "scratchpad from file with no changes should close")
      done();
    });
  }, {noFocus: true});
}

function testUnsaved()
{
  testUnsavedFileCancel();
  testUnsavedFileSave();
  testUnsavedFileDontSave();
}

function testUnsavedFileCancel()
{
  openScratchpad(function(win) {
    win.Scratchpad.setFilename("test.js");
    win.Scratchpad.editor.dirty = true;

    promptButton = win.BUTTON_POSITION_CANCEL;

    win.Scratchpad.close(function() {
      ok(!win.closed, "cancelling dialog shouldn't close scratchpad");
      win.close();
      done();
    });
  }, {noFocus: true});
}

function testUnsavedFileSave()
{
  openScratchpad(function(win) {
    win.Scratchpad.importFromFile(gFile, true, function(status, content) {
      win.Scratchpad.setFilename(gFile.path);

      let text = "new text";
      win.Scratchpad.setText(text);

      promptButton = win.BUTTON_POSITION_SAVE;

      win.Scratchpad.close(function() {
        ok(win.closed, 'pressing "Save" in dialog should close scratchpad');
        readFile(gFile, function(savedContent) {
          is(savedContent, text, 'prompted "Save" worked when closing scratchpad');
          done();
        });
      });
    });
  }, {noFocus: true});
}

function testUnsavedFileDontSave()
{
  openScratchpad(function(win) {
    win.Scratchpad.setFilename(gFile.path);
    win.Scratchpad.editor.dirty = true;

    promptButton = win.BUTTON_POSITION_DONT_SAVE;

    win.Scratchpad.close(function() {
      ok(win.closed, 'pressing "Don\'t Save" in dialog should close scratchpad');
      done();
    });
  }, {noFocus: true});
}

function cleanup()
{
  Services.prompt = oldPrompt;
  gFile.remove(false);
  gFile = null;
}

function createTempFile(name)
{
  let file = FileUtils.getFile("TmpD", [name]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  file.QueryInterface(Ci.nsILocalFile)
  return file;
}

function writeFile(file, content, callback)
{
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(file.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            0644, fout.DEFER_OPEN);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let fileContentStream = converter.convertToInputStream(content);

  NetUtil.asyncCopy(fileContentStream, fout, callback);
}

function readFile(file, callback)
{
  let channel = NetUtil.newChannel(file);
  channel.contentType = "application/javascript";

  NetUtil.asyncFetch(channel, function(inputStream, status) {
    ok(Components.isSuccessCode(status),
       "file was read successfully");

    let content = NetUtil.readInputStreamToString(inputStream,
                                                  inputStream.available());
    callback(content);
  });
}
