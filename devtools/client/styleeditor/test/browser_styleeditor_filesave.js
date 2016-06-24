/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that 'Save' function works.

const TESTCASE_URI_HTML = TEST_BASE_HTTP + "simple.html";
const TESTCASE_URI_CSS = TEST_BASE_HTTP + "simple.css";

var tempScope = {};
Components.utils.import("resource://gre/modules/FileUtils.jsm", tempScope);
Components.utils.import("resource://gre/modules/NetUtil.jsm", tempScope);
var FileUtils = tempScope.FileUtils;
var NetUtil = tempScope.NetUtil;

add_task(function* () {
  let htmlFile = yield copy(TESTCASE_URI_HTML, "simple.html");
  yield copy(TESTCASE_URI_CSS, "simple.css");
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

function copy(srcChromeURL, destFileName) {
  let deferred = defer();
  let destFile = FileUtils.getFile("ProfD", [destFileName]);
  write(read(srcChromeURL), destFile, deferred.resolve);

  return deferred.promise;
}

function read(srcChromeURL) {
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = NetUtil.newChannel({
    uri: srcChromeURL,
    loadUsingSystemPrincipal: true
  });
  let input = channel.open2();
  scriptableStream.init(input);

  let data = "";
  while (input.available()) {
    data = data.concat(scriptableStream.read(input.available()));
  }
  scriptableStream.close();
  input.close();

  return data;
}

function write(data, file, callback) {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);

  converter.charset = "UTF-8";

  let istream = converter.convertToInputStream(data);
  let ostream = FileUtils.openSafeFileOutputStream(file);

  NetUtil.asyncCopy(istream, ostream, function (status) {
    if (!Components.isSuccessCode(status)) {
      info("Couldn't write to " + file.path);
      return;
    }

    callback(file);
  });
}
