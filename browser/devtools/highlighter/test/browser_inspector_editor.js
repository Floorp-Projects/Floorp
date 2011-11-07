/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Kyle Simpson <ksimpson@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

let doc;
let div;
let editorTestSteps;

function doNextStep() {
  editorTestSteps.next();
}

function setupEditorTests()
{
  div = doc.createElement("div");
  div.setAttribute("id", "foobar");
  div.setAttribute("class", "barbaz");
  doc.body.appendChild(div);

  Services.obs.addObserver(setupHTMLPanel, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function setupHTMLPanel()
{
  Services.obs.removeObserver(setupHTMLPanel, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  Services.obs.addObserver(runEditorTests, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
  InspectorUI.toolShow(InspectorUI.treePanel.registrationObject);
}

function runEditorTests()
{
  Services.obs.removeObserver(runEditorTests, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);
  InspectorUI.stopInspecting();
  InspectorUI.inspectNode(doc.body, true);

  // setup generator for async test steps
  editorTestSteps = doEditorTestSteps();

  // add step listeners
  Services.obs.addObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_OPENED, false);
  Services.obs.addObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_CLOSED, false);
  Services.obs.addObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_SAVED, false);

  // start the tests
  doNextStep();
}

function highlighterTrap()
{
  // bug 696107
  Services.obs.removeObserver(highlighterTrap, InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
  ok(false, "Highlighter moved. Shouldn't be here!");
  finishUp();
}

function doEditorTestSteps()
{
  let treePanel = InspectorUI.treePanel;
  let editor = treePanel.treeBrowserDocument.getElementById("attribute-editor");
  let editorInput = treePanel.treeBrowserDocument.getElementById("attribute-editor-input");

  // Step 1: grab and test the attribute-value nodes in the HTML panel, then open editor
  let attrValNode_id = treePanel.treeBrowserDocument.querySelectorAll(".nodeValue.editable[data-attributeName='id']")[0];
  let attrValNode_class = treePanel.treeBrowserDocument.querySelectorAll(".nodeValue.editable[data-attributeName='class']")[0];

  is(attrValNode_id.innerHTML, "foobar", "Step 1: we have the correct `id` attribute-value node in the HTML panel");
  is(attrValNode_class.innerHTML, "barbaz", "we have the correct `class` attribute-value node in the HTML panel");

  // double-click the `id` attribute-value node to open the editor
  executeSoon(function() {
    // firing 2 clicks right in a row to simulate a double-click
    EventUtils.synthesizeMouse(attrValNode_id, 2, 2, {clickCount: 2}, attrValNode_id.ownerDocument.defaultView);
  });

  yield; // End of Step 1


  // Step 2: validate editing session, enter new attribute value into editor, and save input
  ok(InspectorUI.treePanel.editingContext, "Step 2: editor session started");
  let selection = InspectorUI.selection;

  ok(selection, "Selection is: " + selection);

  let editorVisible = editor.classList.contains("editing");
  ok(editorVisible, "editor popup visible");

  // check if the editor popup is "near" the correct position
  let editorDims = editor.getBoundingClientRect();
  let attrValNodeDims = attrValNode_id.getBoundingClientRect();
  let editorPositionOK = (editorDims.left >= (attrValNodeDims.left - editorDims.width - 5)) &&
                          (editorDims.right <= (attrValNodeDims.right + editorDims.width + 5)) &&
                          (editorDims.top >= (attrValNodeDims.top - editorDims.height - 5)) &&
                          (editorDims.bottom <= (attrValNodeDims.bottom + editorDims.height + 5));

  ok(editorPositionOK, "editor position acceptable");

  // check to make sure the attribute-value node being edited is properly highlighted
  let attrValNodeHighlighted = attrValNode_id.classList.contains("editingAttributeValue");
  ok(attrValNodeHighlighted, "`id` attribute-value node is editor-highlighted");

  is(treePanel.editingContext.repObj, div, "editor session has correct reference to div");
  is(treePanel.editingContext.attrObj, attrValNode_id, "editor session has correct reference to `id` attribute-value node in HTML panel");
  is(treePanel.editingContext.attrName, "id", "editor session knows correct attribute-name");

  editorInput.value = "Hello World";
  editorInput.focus();

  Services.obs.addObserver(highlighterTrap,
      InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

  // hit <enter> to save the textbox value
  executeSoon(function() {
    // Extra key to test that keyboard handlers have been removed. bug 696107.
    EventUtils.synthesizeKey("VK_LEFT", {}, attrValNode_id.ownerDocument.defaultView);
    EventUtils.synthesizeKey("VK_RETURN", {}, attrValNode_id.ownerDocument.defaultView);
  });

  // two `yield` statements, to trap both the "SAVED" and "CLOSED" events that will be triggered
  yield;
  yield; // End of Step 2

  // remove this from previous step
  Services.obs.removeObserver(highlighterTrap, InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);

  // Step 3: validate that the previous editing session saved correctly, then open editor on `class` attribute value
  ok(!treePanel.editingContext, "Step 3: editor session ended");
  editorVisible = editor.classList.contains("editing");
  ok(!editorVisible, "editor popup hidden");
  attrValNodeHighlighted = attrValNode_id.classList.contains("editingAttributeValue");
  ok(!attrValNodeHighlighted, "`id` attribute-value node is no longer editor-highlighted");
  is(div.getAttribute("id"), "Hello World", "`id` attribute-value successfully updated");
  is(attrValNode_id.innerHTML, "Hello World", "attribute-value node in HTML panel successfully updated");

  // double-click the `class` attribute-value node to open the editor
  executeSoon(function() {
    // firing 2 clicks right in a row to simulate a double-click
    EventUtils.synthesizeMouse(attrValNode_class, 2, 2, {clickCount: 2}, attrValNode_class.ownerDocument.defaultView);
  });

  yield; // End of Step 3


  // Step 4: enter value into editor, then hit <escape> to discard it
  ok(treePanel.editingContext, "Step 4: editor session started");
  editorVisible = editor.classList.contains("editing");
  ok(editorVisible, "editor popup visible");

  is(treePanel.editingContext.attrObj, attrValNode_class, "editor session has correct reference to `class` attribute-value node in HTML panel");
  is(treePanel.editingContext.attrName, "class", "editor session knows correct attribute-name");

  editorInput.value = "Hello World";
  editorInput.focus();

  // hit <escape> to discard the inputted value
  executeSoon(function() {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, attrValNode_class.ownerDocument.defaultView);
  });

  yield; // End of Step 4


  // Step 5: validate that the previous editing session discarded correctly, then open editor on `id` attribute value again
  ok(!treePanel.editingContext, "Step 5: editor session ended");
  editorVisible = editor.classList.contains("editing");
  ok(!editorVisible, "editor popup hidden");
  is(div.getAttribute("class"), "barbaz", "`class` attribute-value *not* updated");
  is(attrValNode_class.innerHTML, "barbaz", "attribute-value node in HTML panel *not* updated");

  // double-click the `id` attribute-value node to open the editor
  executeSoon(function() {
    // firing 2 clicks right in a row to simulate a double-click
    EventUtils.synthesizeMouse(attrValNode_id, 2, 2, {clickCount: 2}, attrValNode_id.ownerDocument.defaultView);
  });

  yield; // End of Step 5


  // Step 6: validate that editor opened again, then test double-click inside of editor (should do nothing)
  ok(treePanel.editingContext, "Step 6: editor session started");
  editorVisible = editor.classList.contains("editing");
  ok(editorVisible, "editor popup visible");

  // double-click on the editor input box
  executeSoon(function() {
    // firing 2 clicks right in a row to simulate a double-click
    EventUtils.synthesizeMouse(editorInput, 2, 2, {clickCount: 2}, editorInput.ownerDocument.defaultView);

    // since the previous double-click is supposed to do nothing,
    // wait a brief moment, then move on to the next step
    executeSoon(function() {
      doNextStep();
    });
  });

  yield; // End of Step 6


  // Step 7: validate that editing session is still correct, then enter a value and try a click
  //         outside of editor (should cancel the editing session)
  ok(treePanel.editingContext, "Step 7: editor session still going");
  editorVisible = editor.classList.contains("editing");
  ok(editorVisible, "editor popup still visible");

  editorInput.value = "all your base are belong to us";

  // single-click the `class` attribute-value node
  executeSoon(function() {
    EventUtils.synthesizeMouse(attrValNode_class, 2, 2, {}, attrValNode_class.ownerDocument.defaultView);
  });

  yield; // End of Step 7


  // Step 8: validate that the editor was closed and that the editing was not saved
  ok(!treePanel.editingContext, "Step 8: editor session ended");
  editorVisible = editor.classList.contains("editing");
  ok(!editorVisible, "editor popup hidden");
  is(div.getAttribute("id"), "Hello World", "`id` attribute-value *not* updated");
  is(attrValNode_id.innerHTML, "Hello World", "attribute-value node in HTML panel *not* updated");

  // End of Step 8
  executeSoon(finishUp);
}

function finishUp() {
  // end of all steps, so clean up
  Services.obs.removeObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_OPENED, false);
  Services.obs.removeObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_CLOSED, false);
  Services.obs.removeObserver(doNextStep, InspectorUI.INSPECTOR_NOTIFICATIONS.EDITOR_SAVED, false);
  doc = div = null;
  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(setupEditorTests, content);
  }, true);

  content.location = "data:text/html,basic tests for html panel attribute-value editor";
}

