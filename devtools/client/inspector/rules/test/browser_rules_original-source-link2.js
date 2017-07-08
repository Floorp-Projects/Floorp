/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the stylesheet links in the rule view are correct when source maps
// are involved.

const TESTCASE_URI = URL_ROOT + "doc_sourcemaps2.html";
const PREF = "devtools.styleeditor.source-maps-enabled";
const SCSS_LOC = "doc_sourcemaps.scss:4";
const CSS_LOC = "doc_sourcemaps2.css:1";

add_task(function* () {
  info("Setting the " + PREF + " pref to true");
  Services.prefs.setBoolPref(PREF, true);

  yield addTab(TESTCASE_URI);
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("div", inspector);

  yield verifyLinkText(SCSS_LOC, view);

  info("Setting the " + PREF + " pref to false");
  Services.prefs.setBoolPref(PREF, false);
  yield verifyLinkText(CSS_LOC, view);

  info("Setting the " + PREF + " pref to true again");
  Services.prefs.setBoolPref(PREF, true);

  yield testClickingLink(toolbox, view);
  yield checkDisplayedStylesheet(toolbox);

  info("Clearing the " + PREF + " pref");
  Services.prefs.clearUserPref(PREF);
});

function* testClickingLink(toolbox, view) {
  info("Listening for switch to the style editor");
  let onStyleEditorReady = toolbox.once("styleeditor-ready");

  info("Finding the stylesheet link and clicking it");
  let link = getRuleViewLinkByIndex(view, 1);
  link.scrollIntoView();
  link.click();
  yield onStyleEditorReady;
}

function checkDisplayedStylesheet(toolbox) {
  let def = defer();

  let panel = toolbox.getCurrentPanel();
  panel.UI.on("editor-selected", (event, editor) => {
    // The style editor selects the first sheet at first load before
    // selecting the desired sheet.
    if (editor.styleSheet.href.endsWith("scss")) {
      info("Original source editor selected");
      editor.getSourceEditor().then(editorSelected)
        .then(def.resolve, def.reject);
    }
  });

  return def.promise;
}

function editorSelected(editor) {
  let href = editor.styleSheet.href;
  ok(href.endsWith("doc_sourcemaps.scss"),
    "selected stylesheet is correct one");

  let {line} = editor.sourceEditor.getCursor();
  is(line, 3, "cursor is at correct line number in original source");
}

function verifyLinkText(text, view) {
  info("Verifying that the rule-view stylesheet link is " + text);
  let label = getRuleViewLinkByIndex(view, 1)
    .querySelector(".ruleview-rule-source-label");
  return waitForSuccess(function* () {
    return label.textContent == text;
  }, "Link text changed to display correct location: " + text);
}
