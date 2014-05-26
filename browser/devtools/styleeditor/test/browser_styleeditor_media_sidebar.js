/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const PREF = "devtools.styleeditor.showMediaSidebar";

const RESIZE = 300;
const LABELS = ["not all", "all", "(max-width: 400px)"];
const LINE_NOS = [2, 8, 20];

waitForExplicitFinish();

let test = asyncTest(function*() {
  Services.prefs.setBoolPref(PREF, true);

  let {UI} = yield addTabAndOpenStyleEditors(2, null, TESTCASE_URI);

  is(UI.editors.length, 2, "correct number of editors");

  // Test first plain css editor
  let plainEditor = UI.editors[0];
  yield openEditor(plainEditor);
  testPlainEditor(plainEditor);

  // Test editor with @media rules
  let mediaEditor = UI.editors[1];
  yield openEditor(mediaEditor);
  testMediaEditor(mediaEditor);

  // Test resizing and seeing @media matching state change
  let originalWidth = window.outerWidth;
  let originalHeight = window.outerHeight;

  let onMatchesChange = listenForMatchesChange(UI);
  window.resizeTo(RESIZE, RESIZE);
  yield onMatchesChange;

  testMediaMatchChanged(mediaEditor);

  window.resizeTo(originalWidth, originalHeight);
  Services.prefs.clearUserPref(PREF);
});

function testPlainEditor(editor) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, true, "sidebar is hidden on editor without @media");
}

function testMediaEditor(editor) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, false, "sidebar is showing on editor with @media");

  let entries = [...sidebar.querySelectorAll(".media-rule-label")];
  is(entries.length, 3, "three @media rules displayed in sidebar");

  testRule(entries[0], LABELS[0], false, LINE_NOS[0]);
  testRule(entries[1], LABELS[1], true, LINE_NOS[1]);
  testRule(entries[2], LABELS[2], false, LINE_NOS[2]);
}

function testMediaMatchChanged(editor) {
  let sidebar = editor.details.querySelector(".stylesheet-sidebar");

  let cond = sidebar.querySelectorAll(".media-rule-condition")[2];
  is(cond.textContent, "(max-width: 400px)", "third rule condition text is correct");
  ok(!cond.classList.contains("media-condition-unmatched"),
     "media rule is now matched after resizing");
}

function testRule(rule, text, matches, lineno) {
  let cond = rule.querySelector(".media-rule-condition");
  is(cond.textContent, text, "media label is correct for " + text);

  let matched = !cond.classList.contains("media-condition-unmatched");
  ok(matches ? matched : !matched,
     "media rule is " + (matches ? "matched" : "unmatched"));

  let line = rule.querySelector(".media-rule-line");
  is(line.textContent, ":" + lineno, "correct line number shown");
}

/* Helpers */

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function listenForMatchesChange(UI) {
  let deferred = promise.defer();
  UI.once("media-list-changed", () => {
    deferred.resolve();
  })
  return deferred.promise;
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
