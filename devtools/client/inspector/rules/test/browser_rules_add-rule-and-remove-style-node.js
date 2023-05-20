/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that bug 1736412 is fixed
// We press "add new rule", then we remove the style node
// We then try to press "add new rule again"

const TEST_URI = '<div id="testid">Test Node</div>';

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("#testid", inspector);
  await addNewRule(inspector, view);
  await testNewRule(view, 1);
  await testRemoveStyleNode();
  await addNewRule(inspector, view);
  await testNewRule(view, 1);
});

function testNewRule(view) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = ruleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, "#testid", "Selector editor value is as expected");
  info("Escaping from the selector field the change");
  EventUtils.synthesizeKey("KEY_Escape");
}

async function testRemoveStyleNode() {
  info("Removing the style node from the dom");
  const nbStyleSheets = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      content.document.styleSheets[0].ownerNode.remove();
      return content.document.styleSheets.length;
    }
  );
  is(nbStyleSheets, 0, "Style node has been removed");
}
