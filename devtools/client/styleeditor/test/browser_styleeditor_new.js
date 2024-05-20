/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that new sheets can be added and edited.

const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

const TESTCASE_CSS_SOURCE = "body{background-color:red;";

add_task(async function () {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const editor = await createNewStyleSheet(ui, panel.panelWindow);
  await testInitialState(editor);

  const originalHref = editor.styleSheet.href;
  const waitForPropertyChange = onPropertyChange(editor);

  await typeInEditor(editor, panel.panelWindow);

  await waitForPropertyChange;

  await testUpdated(editor, originalHref);
});

function onPropertyChange(editor) {
  return new Promise(resolve => {
    editor.on("property-change", function onProp(property) {
      // wait for text to be entered fully
      const text = editor.sourceEditor.getText();
      if (property == "ruleCount" && text == TESTCASE_CSS_SOURCE + "}") {
        editor.off("property-change", onProp);
        resolve();
      }
    });
  });
}

async function testInitialState(editor) {
  info("Testing the initial state of the new editor");

  ok(editor.sourceLoaded, "new editor is loaded when attached");
  ok(editor.isNew, "new editor has isNew flag");

  if (!editor.sourceEditor.hasFocus()) {
    info("Waiting for stylesheet editor to gain focus");
    await editor.sourceEditor.once("focus");
  }
  ok(editor.sourceEditor.hasFocus(), "new editor has focus");

  await assertRuleCount(editor, 0);

  const color = await getComputedStyleProperty({
    selector: "body",
    name: "background-color",
  });
  is(
    color,
    "rgb(255, 255, 255)",
    "content's background color is initially white"
  );
}

function typeInEditor(editor, panelWindow) {
  return new Promise(resolve => {
    waitForFocus(function () {
      for (const c of TESTCASE_CSS_SOURCE) {
        EventUtils.synthesizeKey(c, {}, panelWindow);
      }
      ok(editor.unsaved, "new editor has unsaved flag");

      resolve();
    }, panelWindow);
  });
}

async function testUpdated(editor, originalHref) {
  info("Testing the state of the new editor after editing it");

  is(
    editor.sourceEditor.getText(),
    TESTCASE_CSS_SOURCE + "}",
    "rule bracket has been auto-closed"
  );

  await assertRuleCount(editor, 1);

  is(editor.styleSheet.href, originalHref, "style sheet href did not change");
}
