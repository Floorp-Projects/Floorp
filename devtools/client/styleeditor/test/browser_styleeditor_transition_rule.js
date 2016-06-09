/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

const NEW_RULE = "body { background-color: purple; }";

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 2, "correct number of editors");

  let editor = ui.editors[0];
  yield openEditor(editor);

  // Set text twice in a row
  let styleChanges = listenForStyleChange(editor.styleSheet);

  editor.sourceEditor.setText(NEW_RULE);
  editor.sourceEditor.setText(NEW_RULE + " ");

  yield styleChanges;

  let rules = yield ContentTask.spawn(gBrowser.selectedBrowser, 0,
  function* (index) {
    let sheet = content.document.styleSheets[index];
    return [...sheet.cssRules].map(rule => rule.cssText);
  });

  // Test that we removed the transition rule, but kept the rule we added
  is(rules.length, 1, "only one rule in stylesheet");
  is(rules[0], NEW_RULE, "stylesheet only contains rule we added");
});

/* Helpers */

function openEditor(editor) {
  let link = editor.summary.querySelector(".stylesheet-name");
  link.click();

  return editor.getSourceEditor();
}

function listenForStyleChange(sheet) {
  let deferred = defer();
  sheet.on("style-applied", deferred.resolve);
  return deferred.promise;
}
