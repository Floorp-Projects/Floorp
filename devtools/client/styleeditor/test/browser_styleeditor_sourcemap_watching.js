/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASE_URI_HTML = TEST_BASE_HTTP + "sourcemaps-watching.html";
const TESTCASE_URI_CSS = TEST_BASE_HTTP + "sourcemap-css/sourcemaps.css";
const TESTCASE_URI_REG_CSS = TEST_BASE_HTTP + "simple.css";
const TESTCASE_URI_SCSS = TEST_BASE_HTTP + "sourcemap-sass/sourcemaps.scss";
const TESTCASE_URI_MAP = TEST_BASE_HTTP + "sourcemap-css/sourcemaps.css.map";
const TESTCASE_SCSS_NAME = "sourcemaps.scss";

const TRANSITIONS_PREF = "devtools.styleeditor.transitions";

const CSS_TEXT = "* { color: blue }";

const {FileUtils} = Components.utils.import("resource://gre/modules/FileUtils.jsm", {});
const {NetUtil} = Components.utils.import("resource://gre/modules/NetUtil.jsm", {});

add_task(function* () {
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      [TRANSITIONS_PREF, false]
    ]}, resolve);
  });

  // copy all our files over so we don't screw them up for other tests
  let HTMLFile = yield copy(TESTCASE_URI_HTML, ["sourcemaps.html"]);
  let CSSFile = yield copy(TESTCASE_URI_CSS,
    ["sourcemap-css", "sourcemaps.css"]);
  yield copy(TESTCASE_URI_SCSS, ["sourcemap-sass", "sourcemaps.scss"]);
  yield copy(TESTCASE_URI_MAP, ["sourcemap-css", "sourcemaps.css.map"]);
  yield copy(TESTCASE_URI_REG_CSS, ["simple.css"]);

  let uri = Services.io.newFileURI(HTMLFile);
  let testcaseURI = uri.resolve("");

  let { ui } = yield openStyleEditorForURL(testcaseURI);

  let editor = ui.editors[1];
  if (getStylesheetNameFor(editor) != TESTCASE_SCSS_NAME) {
    editor = ui.editors[2];
  }

  is(getStylesheetNameFor(editor), TESTCASE_SCSS_NAME, "found scss editor");

  let link = getLinkFor(editor);
  link.click();

  yield editor.getSourceEditor();

  let color = yield getComputedStyleProperty({selector: "div", name: "color"});
  is(color, "rgb(255, 0, 102)", "div is red before saving file");

  // let styleApplied = defer();
  let styleApplied = editor.once("style-applied");

  yield pauseForTimeChange();

  // Edit and save Sass in the editor. This will start off a file-watching
  // process waiting for the CSS file to change.
  yield editSCSS(editor);

  // We can't run Sass or another compiler, so we fake it by just
  // directly changing the CSS file.
  yield editCSSFile(CSSFile);

  info("wrote to CSS file, waiting for style-applied event");

  yield styleApplied;

  color = yield getComputedStyleProperty({selector: "div", name: "color"});
  is(color, "rgb(0, 0, 255)", "div is blue after saving file");
});

function editSCSS(editor) {
  let deferred = defer();

  editor.sourceEditor.setText(CSS_TEXT);

  editor.saveToFile(null, function (file) {
    ok(file, "Scss file should be saved");
    deferred.resolve();
  });

  return deferred.promise;
}

function editCSSFile(CSSFile) {
  return write(CSS_TEXT, CSSFile);
}

function pauseForTimeChange() {
  let deferred = defer();

  // We have to wait for the system time to turn over > 1000 ms so that
  // our file's last change time will show a change. This reflects what
  // would happen in real life with a user manually saving the file.
  setTimeout(deferred.resolve, 2000);

  return deferred.promise;
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
  let destFile = FileUtils.getFile("ProfD", destFilePath);
  return write(read(srcChromeURL), destFile);
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

function write(data, file) {
  let deferred = defer();

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);

  converter.charset = "UTF-8";

  let istream = converter.convertToInputStream(data);
  let ostream = FileUtils.openSafeFileOutputStream(file);

  NetUtil.asyncCopy(istream, ostream, function (status) {
    if (!Components.isSuccessCode(status)) {
      info("Coudln't write to " + file.path);
      return;
    }
    deferred.resolve(file);
  });

  return deferred.promise;
}
