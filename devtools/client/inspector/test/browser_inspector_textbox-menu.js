/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that when right-clicking on various text boxes throughout the inspector does use
// the toolbox's context menu (copy/cut/paste/selectAll/Undo).

add_task(function* () {
  yield addTab(`data:text/html;charset=utf-8,
                <style>h1 { color: red; }</style>
                <h1 id="title">textbox context menu test</h1>`);
  let {toolbox, inspector} = yield openInspector();
  yield selectNode("h1", inspector);

  info("Testing the markup-view tagname");
  let container = yield focusNode("h1", inspector);
  let tag = container.editor.tag;
  tag.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  yield checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view attribute");
  EventUtils.sendKey("tab", inspector.panelWin);
  yield checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view new attribute");
  // It takes 2 tabs to focus the newAttr field, the first one just moves the cursor to
  // the end of the field.
  EventUtils.sendKey("tab", inspector.panelWin);
  EventUtils.sendKey("tab", inspector.panelWin);
  yield checkTextBox(inspector.markup.doc.activeElement, toolbox);

  info("Testing the markup-view textcontent");
  EventUtils.sendKey("tab", inspector.panelWin);
  yield checkTextBox(inspector.markup.doc.activeElement, toolbox);
  // Blur this last markup-view field, since we're moving on to the rule-view next.
  EventUtils.sendKey("escape", inspector.panelWin);

  info("Testing the rule-view selector");
  let ruleView = inspector.ruleview.view;
  let cssRuleEditor = getRuleViewRuleEditor(ruleView, 1);
  EventUtils.synthesizeMouse(cssRuleEditor.selectorText, 0, 0, {}, inspector.panelWin);
  yield checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view property name");
  EventUtils.sendKey("tab", inspector.panelWin);
  yield checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view property value");
  EventUtils.sendKey("tab", inspector.panelWin);
  yield checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Testing the rule-view new property");
  // Tabbing out of the value field triggers a ruleview-changed event that we need to wait
  // for.
  let onRuleViewChanged = once(ruleView, "ruleview-changed");
  EventUtils.sendKey("tab", inspector.panelWin);
  yield onRuleViewChanged;
  yield checkTextBox(inspector.panelDoc.activeElement, toolbox);

  info("Switching to the computed-view");
  let onComputedViewReady = inspector.once("boxmodel-view-updated");
  selectComputedView(inspector);
  yield onComputedViewReady;

  info("Testing the box-model region");
  let margin = inspector.panelDoc.querySelector(".boxmodel-margin.boxmodel-top > span");
  EventUtils.synthesizeMouseAtCenter(margin, {}, inspector.panelWin);
  yield checkTextBox(inspector.panelDoc.activeElement, toolbox);
});

function* checkTextBox(textBox, {textBoxContextMenuPopup}) {
  is(textBoxContextMenuPopup.state, "closed", "The menu is closed");

  info("Simulating context click on the textbox and expecting the menu to open");
  let onContextMenu = once(textBoxContextMenuPopup, "popupshown");
  EventUtils.synthesizeMouse(textBox, 2, 2, {type: "contextmenu", button: 2},
                             textBox.ownerDocument.defaultView);
  yield onContextMenu;

  is(textBoxContextMenuPopup.state, "open", "The menu is now visible");

  info("Closing the menu");
  let onContextMenuHidden = once(textBoxContextMenuPopup, "popuphidden");
  textBoxContextMenuPopup.hidePopup();
  yield onContextMenuHidden;

  is(textBoxContextMenuPopup.state, "closed", "The menu is closed again");
}
