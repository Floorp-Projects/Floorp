/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const TESTCASE_URI_HTML = TEST_BASE_HTTP + "sourcemaps-watching.html";
const TESTCASE_URI_CSS = TEST_BASE_HTTP + "sourcemap-css/sourcemaps.css";
const TESTCASE_URI_REG_CSS = TEST_BASE_HTTP + "simple.css";
const TESTCASE_URI_SCSS = TEST_BASE_HTTP + "sourcemap-sass/sourcemaps.scss";
const TESTCASE_URI_MAP = TEST_BASE_HTTP + "sourcemap-css/sourcemaps.css.map";
const TESTCASE_SCSS_NAME = "sourcemaps.scss";

const TRANSITIONS_PREF = "devtools.styleeditor.transitions";

const CSS_TEXT = "* { color: blue }";

const {FileUtils} = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {});
const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm", {});

add_task(async function() {
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      [TRANSITIONS_PREF, false]
    ]}, resolve);
  });

  // copy all our files over so we don't screw them up for other tests
  const HTMLFile = await copy(TESTCASE_URI_HTML, ["sourcemaps.html"]);
  const CSSFile = await copy(TESTCASE_URI_CSS,
    ["sourcemap-css", "sourcemaps.css"]);
  await copy(TESTCASE_URI_SCSS, ["sourcemap-sass", "sourcemaps.scss"]);
  await copy(TESTCASE_URI_MAP, ["sourcemap-css", "sourcemaps.css.map"]);
  await copy(TESTCASE_URI_REG_CSS, ["simple.css"]);

  const uri = Services.io.newFileURI(HTMLFile);
  const testcaseURI = uri.resolve("");

  const { ui } = await openStyleEditorForURL(testcaseURI);

  let editor = ui.editors[1];
  if (getStylesheetNameFor(editor) != TESTCASE_SCSS_NAME) {
    editor = ui.editors[2];
  }

  is(getStylesheetNameFor(editor), TESTCASE_SCSS_NAME, "found scss editor");

  const link = getLinkFor(editor);
  link.click();

  await editor.getSourceEditor();

  let color = await getComputedStyleProperty({selector: "div", name: "color"});
  is(color, "rgb(255, 0, 102)", "div is red before saving file");

  // let styleApplied = defer();
  const styleApplied = editor.once("style-applied");

  await pauseForTimeChange();

  // Edit and save Sass in the editor. This will start off a file-watching
  // process waiting for the CSS file to change.
  await editSCSS(editor);

  // We can't run Sass or another compiler, so we fake it by just
  // directly changing the CSS file.
  await editCSSFile(CSSFile);

  info("wrote to CSS file, waiting for style-applied event");

  await styleApplied;

  color = await getComputedStyleProperty({selector: "div", name: "color"});
  is(color, "rgb(0, 0, 255)", "div is blue after saving file");

  // Ensure that the editor didn't revert.  Bug 1346662.
  is(editor.sourceEditor.getText(), CSS_TEXT, "edits remain applied");
});

function editSCSS(editor) {
  return new Promise(resolve => {
    editor.sourceEditor.setText(CSS_TEXT);

    editor.saveToFile(null, function(file) {
      ok(file, "Scss file should be saved");
      resolve();
    });
  });
}

function editCSSFile(CSSFile) {
  return write(CSS_TEXT, CSSFile);
}

function pauseForTimeChange() {
  return new Promise(resolve => {
    // We have to wait for the system time to turn over > 1000 ms so that
    // our file's last change time will show a change. This reflects what
    // would happen in real life with a user manually saving the file.
    setTimeout(resolve, 2000);
  });
}

/* Helpers */

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary.querySelector(".stylesheet-name > label")
    .getAttribute("value");
}

function copy(srcChromeURL, destFilePath) {
  const destFile = FileUtils.getFile("ProfD", destFilePath);
  return write(read(srcChromeURL), destFile);
}

function read(srcChromeURL) {
  const scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  const channel = NetUtil.newChannel({
    uri: srcChromeURL,
    loadUsingSystemPrincipal: true
  });
  const input = channel.open2();
  scriptableStream.init(input);

  let data = "";
  while (input.available()) {
    data = data.concat(scriptableStream.read(input.available()));
  }
  scriptableStream.close();
  input.close();

  return data;
}

function write(data, file) {
  return new Promise(resolve => {
    const converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
      .createInstance(Ci.nsIScriptableUnicodeConverter);

    converter.charset = "UTF-8";

    const istream = converter.convertToInputStream(data);
    const ostream = FileUtils.openSafeFileOutputStream(file);

    NetUtil.asyncCopy(istream, ostream, function(status) {
      if (!Components.isSuccessCode(status)) {
        info("Coudln't write to " + file.path);
        return;
      }
      resolve(file);
    });
  });
}
