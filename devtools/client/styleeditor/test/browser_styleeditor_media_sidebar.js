/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules.html";
const MEDIA_PREF = "devtools.styleeditor.showMediaSidebar";

const RESIZE_W = 300;
const RESIZE_H = 450;
const LABELS = [
  "not all",
  "all",
  "(max-width: 550px)",
  "(min-height: 300px) and (max-height: 320px)",
  "(max-width: 750px)",
  "print",
];
const LINE_NOS = [1, 7, 19, 25, 31, 36];
const NEW_RULE = `
  @media (max-width: 750px) {
    div {
      color: blue;
    }

    @media print {
      body {
        filter: grayscale(100%);
      }
    }
  }`;

waitForExplicitFinish();

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 4, "correct number of editors");

  // Test first plain css editor
  const plainEditor = ui.editors[0];
  await openEditor(plainEditor);
  testPlainEditor(plainEditor);

  // Test editor for inline sheet with @media rules
  const inlineMediaEditor = ui.editors[3];
  await openEditor(inlineMediaEditor);
  await testInlineMediaEditor(ui, inlineMediaEditor);

  // Test editor with @media rules
  const mediaEditor = ui.editors[1];
  await openEditor(mediaEditor);
  await testMediaEditor(ui, mediaEditor);

  // Test that sidebar hides when flipping pref
  await testShowHide(ui, mediaEditor);

  // Test adding a rule updates the list
  await testMediaRuleAdded(ui, mediaEditor);

  // Test resizing and seeing @media matching state change
  const originalWidth = window.outerWidth;
  const originalHeight = window.outerHeight;

  const onMatchesChange = listenForMediaChange(ui);
  window.resizeTo(RESIZE_W, RESIZE_H);
  await onMatchesChange;

  testMediaMatchChanged(mediaEditor);

  window.resizeTo(originalWidth, originalHeight);
});

function testPlainEditor(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, true, "sidebar is hidden on editor without @media");
}

async function testInlineMediaEditor(ui, editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, false, "sidebar is showing on editor with @media");

  const entries = sidebar.querySelectorAll(".media-rule-label");
  is(entries.length, 2, "2 @media rules displayed in sidebar");

  await testRule({
    ui,
    editor,
    rule: entries[0],
    conditionText: "screen",
    matches: true,
    line: 2,
  });

  await testRule({
    ui,
    editor,
    rule: entries[1],
    conditionText: "(1px < height < 10000px)",
    matches: true,
    line: 8,
  });
}

async function testMediaEditor(ui, editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, false, "sidebar is showing on editor with @media");

  const entries = [...sidebar.querySelectorAll(".media-rule-label")];
  is(entries.length, 4, "four @media rules displayed in sidebar");

  await testRule({
    ui,
    editor,
    rule: entries[0],
    conditionText: LABELS[0],
    matches: false,
    line: LINE_NOS[0],
  });
  await testRule({
    ui,
    editor,
    rule: entries[1],
    conditionText: LABELS[1],
    matches: true,
    line: LINE_NOS[1],
  });
  await testRule({
    ui,
    editor,
    rule: entries[2],
    conditionText: LABELS[2],
    matches: false,
    line: LINE_NOS[2],
  });
  await testRule({
    ui,
    editor,
    rule: entries[3],
    conditionText: LABELS[3],
    matches: false,
    line: LINE_NOS[3],
  });
}

function testMediaMatchChanged(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");

  const cond = sidebar.querySelectorAll(".media-rule-condition")[2];
  is(
    cond.textContent,
    "(max-width: 550px)",
    "third rule condition text is correct"
  );
  ok(
    !cond.classList.contains("media-condition-unmatched"),
    "media rule is now matched after resizing"
  );
}

async function testShowHide(ui, editor) {
  let sidebarChange = listenForMediaChange(ui);
  Services.prefs.setBoolPref(MEDIA_PREF, false);
  await sidebarChange;

  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, true, "sidebar is hidden after flipping pref");

  sidebarChange = listenForMediaChange(ui);
  Services.prefs.clearUserPref(MEDIA_PREF);
  await sidebarChange;

  is(sidebar.hidden, false, "sidebar is showing after flipping pref back");
}

async function testMediaRuleAdded(ui, editor) {
  await editor.getSourceEditor();
  let text = editor.sourceEditor.getText();
  text += NEW_RULE;

  const listChange = listenForMediaChange(ui);
  editor.sourceEditor.setText(text);
  await listChange;

  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  const entries = [...sidebar.querySelectorAll(".media-rule-label")];
  is(entries.length, 6, "six @media rules after changing text");

  await testRule({
    ui,
    editor,
    rule: entries[4],
    conditionText: LABELS[4],
    matches: false,
    line: LINE_NOS[4],
  });

  await testRule({
    ui,
    editor,
    rule: entries[5],
    conditionText: LABELS[5],
    matches: false,
    line: LINE_NOS[5],
  });
}

/**
 * Run assertion on given rule
 *
 * @param {Object} options
 * @param {StyleEditorUI} options.ui
 * @param {StyleSheetEditor} options.editor: The editor the rule is displayed in
 * @param {Element} options.rule: The rule element in the media sidebar
 * @param {String} options.conditionText: media query condition text
 * @param {Boolean} options.matches: Whether or not the document matches the rule
 * @param {Number} options.line: Line of the rule
 */
async function testRule({ ui, editor, rule, conditionText, matches, line }) {
  const cond = rule.querySelector(".media-rule-condition");
  is(
    cond.textContent,
    conditionText,
    "media label is correct for " + conditionText
  );

  const matched = !cond.classList.contains("media-condition-unmatched");
  ok(
    matches ? matched : !matched,
    "media rule is " + (matches ? "matched" : "unmatched")
  );

  const mediaRuleLine = rule.querySelector(".media-rule-line");
  is(mediaRuleLine.textContent, ":" + line, "correct line number shown");

  info(
    "Check that clicking on the rule jumps to the expected position in the stylesheet"
  );
  rule.click();
  await waitFor(
    () =>
      ui.selectedEditor == editor &&
      editor.sourceEditor.getCursor().line == line - 1
  );
  ok(true, "Jumped to the expected location");
}

/* Helpers */

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function listenForMediaChange(ui) {
  return new Promise(resolve => {
    ui.once("media-list-changed", () => {
      resolve();
    });
  });
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
