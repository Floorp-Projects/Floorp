/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that 'Save' function works.

const TESTCASE_URI_HTML = TEST_BASE_HTTP + "simple.html";
const TESTCASE_URI_CSS = TEST_BASE_HTTP + "simple.css";

const Cc = Components.classes;
const Ci = Components.interfaces;

let tempScope = {};
Components.utils.import("resource://gre/modules/FileUtils.jsm", tempScope);
Components.utils.import("resource://gre/modules/NetUtil.jsm", tempScope);
let FileUtils = tempScope.FileUtils;
let NetUtil = tempScope.NetUtil;

add_task(function* () {
  let htmlFile = yield copy(TESTCASE_URI_HTML, "simple.html");
  let cssFile = yield copy(TESTCASE_URI_CSS, "simple.css");
  let uri = Services.io.newFileURI(htmlFile);
  let filePath = uri.resolve("");

  let { ui } = yield openStyleEditorForURL(filePath);

  let editor = ui.editors[0];
  yield editor.getSourceEditor();

  info("Editing the style sheet.");
  let dirty = editor.sourceEditor.once("dirty-change");
  let beginCursor = {line: 0, ch: 0};
  editor.sourceEditor.replaceText("DIRTY TEXT", beginCursor, beginCursor);

  yield dirty;

  is(editor.sourceEditor.isClean(), false, "Editor is dirty.");
  ok(editor.summary.classList.contains("unsaved"),
     "Star icon is present in the corresponding summary.");

  info("Saving the changes.");
  dirty = editor.sourceEditor.once("dirty-change");

  editor.saveToFile(null, function (file) {
    ok(file, "file should get saved directly when using a file:// URI");
  });

  yield dirty;

  is(editor.sourceEditor.isClean(), true, "Editor is clean.");
  ok(!editor.summary.classList.contains("unsaved"),
     "Star icon is not present in the corresponding summary.");
});

function copy(aSrcChromeURL, aDestFileName)
{
  let deferred = promise.defer();
  let destFile = FileUtils.getFile("ProfD", [aDestFileName]);
  write(read(aSrcChromeURL), destFile, deferred.resolve);

  return deferred.promise;
}

function read(aSrcChromeURL)
{
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = Services.io.newChannel2(aSrcChromeURL,
                                        null,
                                        null,
                                        null,      // aLoadingNode
                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                        null,      // aTriggeringPrincipal
                                        Ci.nsILoadInfo.SEC_NORMAL,
                                        Ci.nsIContentPolicy.TYPE_OTHER);
  let input = channel.open();
  scriptableStream.init(input);

  let data = "";
  while (input.available()) {
    data = data.concat(scriptableStream.read(input.available()));
  }
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
