/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

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

var ScratchpadManager = Scratchpad.ScratchpadManager;
var gFile;

var oldPrompt = Services.prompt;

function test()
{
  waitForExplicitFinish();

  gFile = createTempFile("fileForBug653427.tmp");
  writeFile(gFile, "text", testUnsaved.call(this));

  testNew();
  testSavedFile();

  content.location = "data:text/html,<p>test scratchpad save file prompt on closing";
}

function testNew()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    win.Scratchpad.close();

    ok(win.closed, "new scratchpad window should close without prompting")
    done();
  });
}

function testSavedFile()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    win.Scratchpad.filename = "test.js";
    win.Scratchpad.saved = true;
    win.Scratchpad.close();

    ok(win.closed, "scratchpad from file with no changes should close")
    done();
  });
}

function testUnsaved()
{
  testUnsavedFileCancel();
  testUnsavedFileSave();
  testUnsavedFileDontSave();
}

function testUnsavedFileCancel()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    win.Scratchpad.filename = "test.js";
    win.Scratchpad.saved = false;

    Services.prompt = {
      confirmEx: function() {
        return win.BUTTON_POSITION_CANCEL;
      }
    }

    win.Scratchpad.close();

    ok(!win.closed, "cancelling dialog shouldn't close scratchpad");

    win.close();
    done();
  });
}

function testUnsavedFileSave()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    win.Scratchpad.importFromFile(gFile, true, function(status, content) {
      win.Scratchpad.filename = gFile.path;
      win.Scratchpad.onTextSaved();

      let text = "new text";
      win.Scratchpad.setText(text);

      Services.prompt = {
        confirmEx: function() {
          return win.BUTTON_POSITION_SAVE;
        }
      }

      win.Scratchpad.close(function() {
        readFile(gFile, function(savedContent) {
          is(savedContent, text, 'prompted "Save" worked when closing scratchpad');
          done();
        });
      });

      ok(win.closed, 'pressing "Save" in dialog should close scratchpad');
    });
  });
}

function testUnsavedFileDontSave()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    win.Scratchpad.filename = gFile.path;
    win.Scratchpad.saved = false;

    Services.prompt = {
      confirmEx: function() {
        return win.BUTTON_POSITION_DONT_SAVE;
      }
    }

    win.Scratchpad.close();

    ok(win.closed, 'pressing "Don\'t Save" in dialog should close scratchpad');
    done();
  });
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
