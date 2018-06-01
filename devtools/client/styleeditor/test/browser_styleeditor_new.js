/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that new sheets can be added and edited.

const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

const TESTCASE_CSS_SOURCE = "body{background-color:red;";

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const editor = await createNew(ui, panel.panelWindow);
  await testInitialState(editor);

  const originalHref = editor.styleSheet.href;
  const waitForPropertyChange = onPropertyChange(editor);

  await typeInEditor(editor, panel.panelWindow);

  await waitForPropertyChange;

  testUpdated(editor, originalHref);
});

function createNew(ui, panelWindow) {
  info("Creating a new stylesheet now");

  return new Promise(resolve => {
    ui.once("editor-added", editor => {
      editor.getSourceEditor().then(resolve);
    });

    waitForFocus(function() {
      // create a new style sheet
      const newButton = panelWindow.document
        .querySelector(".style-editor-newButton");
      ok(newButton, "'new' button exists");

      EventUtils.synthesizeMouseAtCenter(newButton, {}, panelWindow);
    }, panelWindow);
  });
}

function onPropertyChange(editor) {
  return new Promise(resolve => {
    editor.styleSheet.on("property-change", function onProp(property) {
      // wait for text to be entered fully
      const text = editor.sourceEditor.getText();
      if (property == "ruleCount" && text == TESTCASE_CSS_SOURCE + "}") {
        editor.styleSheet.off("property-change", onProp);
        resolve();
      }
    });
  });
}

async function testInitialState(editor) {
  info("Testing the initial state of the new editor");

  let summary = editor.summary;

  ok(editor.sourceLoaded, "new editor is loaded when attached");
  ok(editor.isNew, "new editor has isNew flag");

  ok(editor.sourceEditor.hasFocus(), "new editor has focus");

  summary = editor.summary;
  const ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount, 10), 0, "new editor initially shows 0 rules");

  const color = await getComputedStyleProperty({
    selector: "body",
    name: "background-color"
  });
  is(color, "rgb(255, 255, 255)",
     "content's background color is initially white");
}

function typeInEditor(editor, panelWindow) {
  return new Promise(resolve => {
    waitForFocus(function() {
      for (const c of TESTCASE_CSS_SOURCE) {
        EventUtils.synthesizeKey(c, {}, panelWindow);
      }
      ok(editor.unsaved, "new editor has unsaved flag");

      resolve();
    }, panelWindow);
  });
}

function testUpdated(editor, originalHref) {
  info("Testing the state of the new editor after editing it");

  is(editor.sourceEditor.getText(), TESTCASE_CSS_SOURCE + "}",
     "rule bracket has been auto-closed");

  const ruleCount = editor.summary.querySelector(".stylesheet-rule-count")
    .textContent;
  is(parseInt(ruleCount, 10), 1,
     "new editor shows 1 rule after modification");

  is(editor.styleSheet.href, originalHref,
     "style sheet href did not change");
}
