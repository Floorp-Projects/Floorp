/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Task.jsm");
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

const TESTCASE_URI_HTML = TEST_BASE + "sourcemaps-watching.html";
const TESTCASE_URI_CSS = TEST_BASE + "sourcemap-css/sourcemaps.css";
const TESTCASE_URI_REG_CSS = TEST_BASE + "simple.css";
const TESTCASE_URI_SCSS = TEST_BASE + "sourcemap-sass/sourcemaps.scss";
const TESTCASE_URI_MAP = TEST_BASE + "sourcemap-css/sourcemaps.css.map";
const TESTCASE_SCSS_NAME = "sourcemaps.scss";

const SOURCE_MAP_PREF = "devtools.styleeditor.source-maps-enabled";
const TRANSITIONS_PREF = "devtools.styleeditor.transitions";

const CSS_TEXT = "* { color: blue }";

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

  Services.prefs.setBoolPref(SOURCE_MAP_PREF, true);
  Services.prefs.setBoolPref(TRANSITIONS_PREF, false);

  Task.spawn(function() {
    // copy all our files over so we don't screw them up for other tests
    let HTMLFile = yield copy(TESTCASE_URI_HTML, ["sourcemaps.html"]);
    let CSSFile = yield copy(TESTCASE_URI_CSS, ["sourcemap-css", "sourcemaps.css"]);
    yield copy(TESTCASE_URI_SCSS, ["sourcemap-sass", "sourcemaps.scss"]);
    yield copy(TESTCASE_URI_MAP, ["sourcemap-css", "sourcemaps.css.map"]);
    yield copy(TESTCASE_URI_REG_CSS, ["simple.css"]);

    let uri = Services.io.newFileURI(HTMLFile);
    let testcaseURI = uri.resolve("");

    let editor = yield openEditor(testcaseURI);

    let element = content.document.querySelector("div");
    let style = content.getComputedStyle(element, null);

    is(style.color, "rgb(255, 0, 102)", "div is red before saving file");

    editor.styleSheet.relatedStyleSheet.once("style-applied", function() {
      is(style.color, "rgb(0, 0, 255)", "div is blue after saving file");
      finishUp();
    });

    yield pauseForTimeChange();

    // Edit and save Sass in the editor. This will start off a file-watching
    // process waiting for the CSS file to change.
    yield editSCSS(editor);

    // We can't run Sass or another compiler, so we fake it by just
    // directly changing the CSS file.
    yield editCSSFile(CSSFile);

    info("wrote to CSS file");
  })
}

function openEditor(testcaseURI) {
  let deferred = promise.defer();

  addTabAndOpenStyleEditors(3, panel => {
    let UI = panel.UI;

    // wait for 5 editors - 1 for first style sheet, 2 for the
    // generated style sheets, and 2 for original source after it
    // loads and replaces the generated style sheets.
    let editor = UI.editors[1];
    if (getStylesheetNameFor(editor) != TESTCASE_SCSS_NAME) {
      editor = UI.editors[2];
    }
    is(getStylesheetNameFor(editor), TESTCASE_SCSS_NAME, "found scss editor");

    let link = getLinkFor(editor);
    link.click();

    editor.getSourceEditor().then(deferred.resolve);
  });
  content.location = testcaseURI;

  return deferred.promise;
}

function editSCSS(editor) {
  let deferred = promise.defer();

  let pos = {line: 0, ch: 0};
  editor.sourceEditor.replaceText(CSS_TEXT, pos, pos);

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
  let deferred = promise.defer();

  // We have to wait for the system time to turn over > 1000 ms so that
  // our file's last change time will show a change. This reflects what
  // would happen in real life with a user manually saving the file.
  setTimeout(deferred.resolve, 2000);

  return deferred.promise;
}

function finishUp() {
  Services.prefs.clearUserPref(SOURCE_MAP_PREF);
  Services.prefs.clearUserPref(TRANSITIONS_PREF);
  finish();
}

/* Helpers */

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary.querySelector(".stylesheet-name > label")
         .getAttribute("value")
}

function copy(aSrcChromeURL, aDestFilePath)
{
  let destFile = FileUtils.getFile("ProfD", aDestFilePath);
  return write(read(aSrcChromeURL), destFile);
}

function read(aSrcChromeURL)
{
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = Services.io.newChannel(aSrcChromeURL, null, null);
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

function write(aData, aFile)
{
  let deferred = promise.defer();

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
    deferred.resolve(aFile);
  });

  return deferred.promise;
}
