/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the stylesheet links in the rule view are correct when source maps
// are involved.

const TESTCASE_URI = URL_ROOT + "doc_sourcemaps.html";
const PREF = "devtools.source-map.client-service.enabled";
const SCSS_LOC = "doc_sourcemaps.scss:4";
const CSS_LOC = "doc_sourcemaps.css:1";

add_task(async function() {
  info("Setting the " + PREF + " pref to true");
  Services.prefs.setBoolPref(PREF, true);

  await addTab(TESTCASE_URI);
  const {toolbox, inspector, view} = await openRuleView();

  info("Selecting the test node");
  await selectNode("div", inspector);

  await verifyLinkText(SCSS_LOC, view);

  info("Setting the " + PREF + " pref to false");
  Services.prefs.setBoolPref(PREF, false);
  await verifyLinkText(CSS_LOC, view);

  info("Setting the " + PREF + " pref to true again");
  Services.prefs.setBoolPref(PREF, true);

  await testClickingLink(toolbox, view);
  await checkDisplayedStylesheet(toolbox);

  info("Clearing the " + PREF + " pref");
  Services.prefs.clearUserPref(PREF);
});

async function testClickingLink(toolbox, view) {
  info("Listening for switch to the style editor");
  const onStyleEditorReady = toolbox.once("styleeditor-ready");

  info("Finding the stylesheet link and clicking it");
  const link = getRuleViewLinkByIndex(view, 1);
  link.scrollIntoView();
  link.click();
  await onStyleEditorReady;
}

function checkDisplayedStylesheet(toolbox) {
  const panel = toolbox.getCurrentPanel();
  return new Promise((resolve, reject) => {
    panel.UI.on("editor-selected", editor => {
      // The style editor selects the first sheet at first load before
      // selecting the desired sheet.
      if (editor.styleSheet.href.endsWith("scss")) {
        info("Original source editor selected");
        editor.getSourceEditor().then(editorSelected)
          .then(resolve, reject);
      }
    });
  });
}

function editorSelected(editor) {
  const href = editor.styleSheet.href;
  ok(href.endsWith("doc_sourcemaps.scss"),
    "selected stylesheet is correct one");

  const {line} = editor.sourceEditor.getCursor();
  is(line, 3, "cursor is at correct line number in original source");
}

function verifyLinkText(text, view) {
  info("Verifying that the rule-view stylesheet link is " + text);
  const label = getRuleViewLinkByIndex(view, 1)
    .querySelector(".ruleview-rule-source-label");
  return waitForSuccess(function() {
    return label.textContent == text && label.getAttribute("title") === URL_ROOT + text;
  }, "Link text changed to display correct location: " + text);
}
