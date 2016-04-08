/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";

const contents = {
  "sourcemaps.scss": [
    "",
    "$paulrougetpink: #f06;",
    "",
    "div {",
    "  color: $paulrougetpink;",
    "}",
    "",
    "span {",
    "  background-color: #EEE;",
    "}"
  ].join("\n"),
  "contained.scss": [
    "$pink: #f06;",
    "",
    "#header {",
    "  color: $pink;",
    "}"
  ].join("\n"),
  "sourcemaps.css": [
    "div {",
    "  color: #ff0066; }",
    "",
    "span {",
    "  background-color: #EEE; }",
    "",
    "/*# sourceMappingURL=sourcemaps.css.map */"
  ].join("\n"),
  "contained.css": [
    "#header {",
    "  color: #f06; }",
    "",
    "/*# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJma" +
    "WxlIjoiIiwic291cmNlcyI6WyJzYXNzL2NvbnRhaW5lZC5zY3NzIl0sIm5hbWVzIjpbXSwi" +
    "bWFwcGluZ3MiOiJBQUVBO0VBQ0UsT0FISyIsInNvdXJjZXNDb250ZW50IjpbIiRwaW5rOiA" +
    "jZjA2O1xuXG4jaGVhZGVyIHtcbiAgY29sb3I6ICRwaW5rO1xufSJdfQ==*/"
  ].join("\n"),
  "test-stylus.styl": [
    "paulrougetpink = #f06;",
    "",
    "div",
    "  color: paulrougetpink",
    "",
    "span",
    "  background-color: #EEE",
    ""
  ].join("\n"),
  "test-stylus.css": [
    "div {",
    "  color: #f06;",
    "}",
    "span {",
    "  background-color: #eee;",
    "}",
    "/*# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb" +
    "3VyY2VzIjpbInRlc3Qtc3R5bHVzLnN0eWwiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFB" +
    "RUE7RUFDRSxPQUFPLEtBQVA7O0FBRUY7RUFDRSxrQkFBa0IsS0FBbEIiLCJmaWxlIjoidGV" +
    "zdC1zdHlsdXMuY3NzIiwic291cmNlc0NvbnRlbnQiOlsicGF1bHJvdWdldHBpbmsgPSAjZj" +
    "A2O1xuXG5kaXZcbiAgY29sb3I6IHBhdWxyb3VnZXRwaW5rXG5cbnNwYW5cbiAgYmFja2dyb" +
    "3VuZC1jb2xvcjogI0VFRVxuIl19 */"
  ].join("\n")
};

const cssNames = ["sourcemaps.css", "contained.css", "test-stylus.css"];
const origNames = ["sourcemaps.scss", "contained.scss", "test-stylus.styl"];

waitForExplicitFinish();

add_task(function* () {
  let {ui} = yield openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 4,
    "correct number of editors with source maps enabled");

  // Test first plain css editor
  testFirstEditor(ui.editors[0]);

  // Test Scss editors
  yield testEditor(ui.editors[1], origNames);
  yield testEditor(ui.editors[2], origNames);
  yield testEditor(ui.editors[3], origNames);

  // Test disabling original sources
  yield togglePref(ui);

  is(ui.editors.length, 4, "correct number of editors after pref toggled");

  // Test CSS editors
  yield testEditor(ui.editors[1], cssNames);
  yield testEditor(ui.editors[2], cssNames);
  yield testEditor(ui.editors[3], cssNames);

  Services.prefs.clearUserPref(PREF);
});

function testFirstEditor(editor) {
  let name = getStylesheetNameFor(editor);
  is(name, "simple.css", "First style sheet display name is correct");
}

function testEditor(editor, possibleNames) {
  let name = getStylesheetNameFor(editor);
  ok(possibleNames.indexOf(name) >= 0, name + " editor name is correct");

  return openEditor(editor).then(() => {
    let expectedText = contents[name];

    let text = editor.sourceEditor.getText();

    is(text, expectedText, name + " editor contains expected text");
  });
}

/* Helpers */

function togglePref(UI) {
  let editorsPromise = UI.once("stylesheets-reset");
  let selectedPromise = UI.once("editor-selected");

  Services.prefs.setBoolPref(PREF, false);

  return promise.all([editorsPromise, selectedPromise]);
}

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary.querySelector(".stylesheet-name > label")
    .getAttribute("value");
}
