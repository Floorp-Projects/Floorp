/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that when right-clicking on various text boxes throughout the inspector does use
// the toolbox's context menu (copy/cut/paste/selectAll/Undo).

add_task(async function() {
  await addTab(`data:text/html;charset=utf-8,
                <style>h1 { color: red; }</style>
                <h1 id="title">textbox context menu test</h1>`);
  const { toolbox, inspector } = await openInspector();
  await selectNode("h1", inspector);

  info("Testing the markup-view tagname");
  const container = await focusNode("h1", inspector);
  const tag = container.editor.tag;
  tag.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  await checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view attribute");
  EventUtils.sendKey("tab", inspector.panelWin);
  await checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view new attribute");
  // It takes 2 tabs to focus the newAttr field, the first one just moves the cursor to
  // the end of the field.
  EventUtils.sendKey("tab", inspector.panelWin);
  EventUtils.sendKey("tab", inspector.panelWin);
  await checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view textcontent");
  EventUtils.sendKey("tab", inspector.panelWin);
  await checkTextBox(inspector.markup.doc.activeElement, toolbox);
  // Blur this last markup-view field, since we're moving on to the rule-view next.
  EventUtils.sendKey("escape", inspector.panelWin);

  info("Testing the rule-view selector");
  const ruleView = inspector.getPanel("ruleview").view;
  const cssRuleEditor = getRuleViewRuleEditor(ruleView, 1);
  EventUtils.synthesizeMouseAtCenter(
    cssRuleEditor.selectorText,
    {},
    inspector.panelWin
  );
  await checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view property name");
  EventUtils.sendKey("tab", inspector.panelWin);
  await checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view property value");
  EventUtils.sendKey("tab", inspector.panelWin);
  await checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view new property");
  // Tabbing out of the value field triggers a ruleview-changed event that we need to wait
  // for.
  const onRuleViewChanged = once(ruleView, "ruleview-changed");
  EventUtils.sendKey("tab", inspector.panelWin);
  await onRuleViewChanged;
  await checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Switching to the layout-view");
  const onBoxModelUpdated = inspector.once("boxmodel-view-updated");
  selectLayoutView(inspector);
  await onBoxModelUpdated;

  info("Testing the box-model region");
  const margin = inspector.panelDoc.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );
  EventUtils.synthesizeMouseAtCenter(margin, {}, inspector.panelWin);
  await checkTextBox(inspector.panelDoc.activeElement, toolbox);

  // Move the mouse out of the box-model region to avoid triggering the box model
  // highlighter.
  EventUtils.synthesizeMouseAtCenter(tag, {}, inspector.panelWin);
});

async function checkTextBox(textBox, toolbox) {
  let textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is closed");

  info(
    "Simulating context click on the textbox and expecting the menu to open"
  );
  const onContextMenu = toolbox.once("menu-open");
  synthesizeContextMenuEvent(textBox);
  await onContextMenu;

  textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(textboxContextMenu, "The menu is now visible");

  info("Closing the menu");
  const onContextMenuHidden = toolbox.once("menu-close");
  textboxContextMenu.hidePopup();
  await onContextMenuHidden;

  textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is closed again");
}
