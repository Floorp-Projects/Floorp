/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps-inline.html";
const PREF = "devtools.source-map.client-service.enabled";

const sassContent = `body {
  background-color: black;
  & > h1 {
    color: white;
  }
}
`;

const cssContent =
  `body {
  background-color: black;
}
body > h1 {
  color: white;
}
` +
  "/*# sourceMappingURL=data:application/json;base64,ewoidmVyc2lvbiI6IDMsCiJtY" +
  "XBwaW5ncyI6ICJBQUFBLElBQUs7RUFDSCxnQkFBZ0IsRUFBRSxLQUFLO0VBQ3ZCLFNBQU87SUFD" +
  "TCxLQUFLLEVBQUUsS0FBSyIsCiJzb3VyY2VzIjogWyJ0ZXN0LnNjc3MiXSwKInNvdXJjZXNDb25" +
  "0ZW50IjogWyJib2R5IHtcbiAgYmFja2dyb3VuZC1jb2xvcjogYmxhY2s7XG4gICYgPiBoMSB7XG" +
  "4gICAgY29sb3I6IHdoaXRlO1xuICB9XG59XG4iXSwKIm5hbWVzIjogW10sCiJmaWxlIjogInRlc" +
  "3QuY3NzIgp9Cg== */";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(
    ui.editors.length,
    1,
    "correct number of editors with source maps enabled"
  );

  await testEditor(ui.editors[0], "test.scss", sassContent);

  // Test disabling original sources
  await togglePref(ui);

  is(ui.editors.length, 1, "correct number of editors after pref toggled");

  // Test CSS editors
  await testEditor(ui.editors[0], "<inline style sheet #1>", cssContent);

  Services.prefs.clearUserPref(PREF);
});

async function testEditor(editor, expectedName, expectedText) {
  const name = getStylesheetNameFor(editor);
  is(expectedName, name, name + " editor name is correct");

  await openEditor(editor);
  const text = editor.sourceEditor.getText();
  is(text, expectedText, name + " editor contains expected text");
}

/* Helpers */

function togglePref(UI) {
  const editorsPromise = UI.once("stylesheets-refreshed");
  const selectedPromise = UI.once("editor-selected");

  Services.prefs.setBoolPref(PREF, false);

  return Promise.all([editorsPromise, selectedPromise]);
}

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary
    .querySelector(".stylesheet-name > label")
    .getAttribute("value");
}
