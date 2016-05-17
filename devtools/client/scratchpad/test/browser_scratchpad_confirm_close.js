/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 653427 */

var tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
var NetUtil = tempScope.NetUtil;
var FileUtils = tempScope.FileUtils;

// only finish() when correct number of tests are done
const expected = 9;
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
    confirmEx: function () {
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
  openScratchpad(function (win) {
    win.Scratchpad.close(function () {
      ok(win.closed, "new scratchpad window should close without prompting");
      done();
    });
  }, {noFocus: true});
}

function testSavedFile()
{
  openScratchpad(function (win) {
    win.Scratchpad.filename = "test.js";
    win.Scratchpad.editor.dirty = false;
    win.Scratchpad.close(function () {
      ok(win.closed, "scratchpad from file with no changes should close");
      done();
    });
  }, {noFocus: true});
}

function testUnsaved()
{
  function setFilename(aScratchpad, aFile) {
    aScratchpad.setFilename(aFile);
  }

  testUnsavedFileCancel(setFilename);
  testUnsavedFileSave(setFilename);
  testUnsavedFileDontSave(setFilename);
  testCancelAfterLoad();

  function mockSaveFile(aScratchpad) {
    let SaveFileStub = function (aCallback) {
      /*
       * An argument for aCallback must pass Components.isSuccessCode
       *
       * A version of isSuccessCode in JavaScript:
       *  function isSuccessCode(returnCode) {
       *    return (returnCode & 0x80000000) == 0;
       *  }
       */
      aCallback(1);
    };

    aScratchpad.saveFile = SaveFileStub;
  }

  // Run these tests again but this time without setting a filename to
  // test that Scratchpad always asks for confirmation on dirty editor.
  testUnsavedFileCancel(mockSaveFile);
  testUnsavedFileSave(mockSaveFile);
  testUnsavedFileDontSave();
}

function testUnsavedFileCancel(aCallback = function () {})
{
  openScratchpad(function (win) {
    aCallback(win.Scratchpad, "test.js");
    win.Scratchpad.editor.dirty = true;

    promptButton = win.BUTTON_POSITION_CANCEL;

    win.Scratchpad.close(function () {
      ok(!win.closed, "cancelling dialog shouldn't close scratchpad");
      win.close();
      done();
    });
  }, {noFocus: true});
}

// Test a regression where our confirmation dialog wasn't appearing
// after openFile calls. See bug 801982.
function testCancelAfterLoad()
{
  openScratchpad(function (win) {
    win.Scratchpad.setRecentFile(gFile);
    win.Scratchpad.openFile(0);
    win.Scratchpad.editor.dirty = true;
    promptButton = win.BUTTON_POSITION_CANCEL;

    let EventStub = {
      called: false,
      preventDefault: function () {
        EventStub.called = true;
      }
    };

    win.Scratchpad.onClose(EventStub, function () {
      ok(!win.closed, "cancelling dialog shouldn't close scratchpad");
      ok(EventStub.called, "aEvent.preventDefault was called");

      win.Scratchpad.editor.dirty = false;
      win.close();
      done();
    });
  }, {noFocus: true});
}

function testUnsavedFileSave(aCallback = function () {})
{
  openScratchpad(function (win) {
    win.Scratchpad.importFromFile(gFile, true, function (status, content) {
      aCallback(win.Scratchpad, gFile.path);

      let text = "new text";
      win.Scratchpad.setText(text);

      promptButton = win.BUTTON_POSITION_SAVE;

      win.Scratchpad.close(function () {
        ok(win.closed, 'pressing "Save" in dialog should close scratchpad');
        readFile(gFile, function (savedContent) {
          is(savedContent, text, 'prompted "Save" worked when closing scratchpad');
          done();
        });
      });
    });
  }, {noFocus: true});
}

function testUnsavedFileDontSave(aCallback = function () {})
{
  openScratchpad(function (win) {
    aCallback(win.Scratchpad, gFile.path);
    win.Scratchpad.editor.dirty = true;

    promptButton = win.BUTTON_POSITION_DONT_SAVE;

    win.Scratchpad.close(function () {
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
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  file.QueryInterface(Ci.nsILocalFile);
  return file;
}

function writeFile(file, content, callback)
{
  let fout = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
  fout.init(file.QueryInterface(Ci.nsILocalFile), 0x02 | 0x08 | 0x20,
            0o644, fout.DEFER_OPEN);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let fileContentStream = converter.convertToInputStream(content);

  NetUtil.asyncCopy(fileContentStream, fout, callback);
}

function readFile(file, callback)
{
  let channel = NetUtil.newChannel({
    uri: NetUtil.newURI(file),
    loadUsingSystemPrincipal: true});
  channel.contentType = "application/javascript";

  NetUtil.asyncFetch(channel, function (inputStream, status) {
    ok(Components.isSuccessCode(status),
       "file was read successfully");

    let content = NetUtil.readInputStreamToString(inputStream,
                                                  inputStream.available());
    callback(content);
  });
}
