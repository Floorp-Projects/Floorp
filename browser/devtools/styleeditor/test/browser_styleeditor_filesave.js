/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI_HTML = TEST_BASE + "simple.html";
const TESTCASE_URI_CSS = TEST_BASE + "simple.css";

const Cc = Components.classes;
const Ci = Components.interfaces;

let tempScope = {};
Components.utils.import("resource://gre/modules/FileUtils.jsm", tempScope);
Components.utils.import("resource://gre/modules/NetUtil.jsm", tempScope);
let FileUtils = tempScope.FileUtils;
let NetUtil = tempScope.NetUtil;

function test()
{
  waitForExplicitFinish();

  copy(TESTCASE_URI_HTML, "simple.html", function(htmlFile) {
    copy(TESTCASE_URI_CSS, "simple.css", function(cssFile) {
      addTabAndOpenStyleEditor(function(panel) {
        let UI = panel.UI;
        UI.on("editor-added", function(event, editor) {
          if (editor.styleSheet.styleSheetIndex != 0) {
            return;  // we want to test against the first stylesheet
          }
          let editor = UI.editors[0];
          editor.getSourceEditor().then(runTests.bind(this, editor));
        })
      });

      let uri = Services.io.newFileURI(htmlFile);
      let filePath = uri.resolve("");
      content.location = filePath;
    });
  });
}

function runTests(editor)
{
  editor.sourceEditor.once("dirty-change", () => {
    is(editor.sourceEditor.isClean(), false, "Editor is dirty.");
    ok(editor.summary.classList.contains("unsaved"),
       "Star icon is present in the corresponding summary.");
  });
  let beginCursor = {line: 0, ch: 0};
  editor.sourceEditor.replaceText("DIRTY TEXT", beginCursor, beginCursor);

  editor.sourceEditor.once("dirty-change", () => {
    is(editor.sourceEditor.isClean(), true, "Editor is clean.");
    ok(!editor.summary.classList.contains("unsaved"),
       "Star icon is not present in the corresponding summary.");
    finish();
  });
  editor.saveToFile(null, function (file) {
    ok(file, "file should get saved directly when using a file:// URI");
  });
}

function copy(aSrcChromeURL, aDestFileName, aCallback)
{
  let destFile = FileUtils.getFile("ProfD", [aDestFileName]);
  write(read(aSrcChromeURL), destFile, aCallback);
}

function read(aSrcChromeURL)
{
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = Services.io.newChannel(aSrcChromeURL, null, null);
  let input = channel.open();
  scriptableStream.init(input);

  let data = scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  return data;
}

function write(aData, aFile, aCallback)
{
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);

  converter.charset = "UTF-8";

  let istream = converter.convertToInputStream(aData);
  let ostream = FileUtils.openSafeFileOutputStream(aFile);

  NetUtil.asyncCopy(istream, ostream, function(status) {
    if (!Components.isSuccessCode(status)) {
      info("Coudln't write to " + aFile.path);
      return;
    }

    aCallback(aFile);
  });
}
