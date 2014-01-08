/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";

function test()
{
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF, true);

  // wait for 3 editors - 1 for first style sheet, 1 for the
  // generated style sheet, and 1 for original source after it
  // loads and replaces the generated style sheet.
  addTabAndOpenStyleEditors(3, panel => runTests(panel.UI));

  content.location = TESTCASE_URI;
}

function runTests(UI)
{
  is(UI.editors.length, 2);

  let firstEditor = UI.editors[0];
  testFirstEditor(firstEditor);

  let ScssEditor = UI.editors[1];

  let link = getStylesheetNameLinkFor(ScssEditor);
  link.click();

  ScssEditor.getSourceEditor().then(() => {
    testScssEditor(ScssEditor);

    togglePref(UI);
  });
}

function togglePref(UI) {
  let count = 0;

  UI.on("editor-added", (event, editor) => {
    if (++count == 2) {
      testTogglingPref(UI);
    }
  })

  Services.prefs.setBoolPref(PREF, false);
}

function testTogglingPref(UI) {
  is(UI.editors.length, 2, "correct number of editors after pref toggled");

  let CSSEditor = UI.editors[1];

  let link = getStylesheetNameLinkFor(CSSEditor);
  link.click();

  CSSEditor.getSourceEditor().then(() => {
    testCSSEditor(CSSEditor);

    finishUp();
  })
}

function testFirstEditor(editor) {
  let name = getStylesheetNameFor(editor);
  is(name, "simple.css", "First style sheet display name is correct");
}

function testScssEditor(editor) {
  let name = getStylesheetNameFor(editor);
  is(name, "sourcemaps.scss", "Original source display name is correct");

  let text = editor.sourceEditor.getText();

  is(text, "\n\
$paulrougetpink: #f06;\n\
\n\
div {\n\
  color: $paulrougetpink;\n\
}\n\
\n\
span {\n\
  background-color: #EEE;\n\
}", "Original source text is correct");
}

function testCSSEditor(editor) {
  let name = getStylesheetNameFor(editor);
  is(name, "sourcemaps.css", "CSS source display name is correct");

  let text = editor.sourceEditor.getText();

  is(text, "div {\n\
  color: #ff0066; }\n\
\n\
span {\n\
  background-color: #EEE; }\n\
\n\
/*# sourceMappingURL=sourcemaps.css.map */", "CSS text is correct");
}

/* Helpers */
function getStylesheetNameLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary.querySelector(".stylesheet-name > label")
         .getAttribute("value")
}

function finishUp() {
  Services.prefs.clearUserPref(PREF);
  finish();
}