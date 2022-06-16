/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that 'Save' function works.

const TESTCASE_URI_HTML = TEST_BASE_HTTP + "simple.html";
const TESTCASE_URI_CSS = TEST_BASE_HTTP + "simple.css";

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

add_task(async function() {
  const htmlFile = await copy(TESTCASE_URI_HTML, "simple.html");
  await copy(TESTCASE_URI_CSS, "simple.css");
  const uri = Services.io.newFileURI(htmlFile);
  const filePath = uri.resolve("");

  const { ui } = await openStyleEditorForURL(filePath);

  const editor = ui.editors[0];
  await editor.getSourceEditor();

  info("Editing the style sheet.");
  let dirty = editor.sourceEditor.once("dirty-change");
  const beginCursor = { line: 0, ch: 0 };
  editor.sourceEditor.replaceText("DIRTY TEXT", beginCursor, beginCursor);

  await dirty;

  is(editor.sourceEditor.isClean(), false, "Editor is dirty.");
  ok(
    editor.summary.classList.contains("unsaved"),
    "Star icon is present in the corresponding summary."
  );

  info("Saving the changes.");
  dirty = editor.sourceEditor.once("dirty-change");

  editor.saveToFile(null, function(file) {
    ok(file, "file should get saved directly when using a file:// URI");
  });

  await dirty;

  is(editor.sourceEditor.isClean(), true, "Editor is clean.");
  ok(
    !editor.summary.classList.contains("unsaved"),
    "Star icon is not present in the corresponding summary."
  );
});

function copy(srcChromeURL, destFileName) {
  return new Promise(resolve => {
    const destFile = FileUtils.getFile("ProfD", [destFileName]);
    write(read(srcChromeURL), destFile, resolve);
  });
}

function read(srcChromeURL) {
  const scriptableStream = Cc[
    "@mozilla.org/scriptableinputstream;1"
  ].getService(Ci.nsIScriptableInputStream);

  const channel = NetUtil.newChannel({
    uri: srcChromeURL,
    loadUsingSystemPrincipal: true,
  });
  const input = channel.open();
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
  const istream = getInputStream(data);
  const ostream = FileUtils.openSafeFileOutputStream(file);

  NetUtil.asyncCopy(istream, ostream, function(status) {
    if (!Components.isSuccessCode(status)) {
      info("Couldn't write to " + file.path);
      return;
    }

    callback(file);
  });
}
