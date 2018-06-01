/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "media-rules-sourcemaps.html";
const MAP_PREF = "devtools.source-map.client-service.enabled";

const LABELS = ["screen and (max-width: 320px)",
                "screen and (min-width: 1200px)"];
const LINE_NOS = [5, 8];

waitForExplicitFinish();

add_task(async function() {
  Services.prefs.setBoolPref(MAP_PREF, true);

  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 1, "correct number of editors");

  // Test editor with @media rules
  const mediaEditor = ui.editors[0];
  await openEditor(mediaEditor);
  testMediaEditor(mediaEditor);

  Services.prefs.clearUserPref(MAP_PREF);
});

function testMediaEditor(editor) {
  const sidebar = editor.details.querySelector(".stylesheet-sidebar");
  is(sidebar.hidden, false, "sidebar is showing on editor with @media");

  const entries = [...sidebar.querySelectorAll(".media-rule-label")];
  is(entries.length, 2, "two @media rules displayed in sidebar");

  testRule(entries[0], LABELS[0], LINE_NOS[0]);
  testRule(entries[1], LABELS[1], LINE_NOS[1]);
}

function testRule(rule, text, lineno) {
  const cond = rule.querySelector(".media-rule-condition");
  is(cond.textContent, text, "media label is correct for " + text);

  const line = rule.querySelector(".media-rule-line");
  is(line.textContent, ":" + lineno, "correct line number shown");
}

/* Helpers */

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
