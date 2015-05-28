/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadHelperScript("helper_edits.js");

// Test context menu enabled / disabled state in editor

add_task(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  ok (projecteditor, "ProjectEditor has loaded");

  let {textEditorContextMenuPopup} = projecteditor;

  let cmdDelete = textEditorContextMenuPopup.querySelector("[command=cmd_delete]");
  let cmdSelectAll = textEditorContextMenuPopup.querySelector("[command=cmd_selectAll]");
  let cmdCut = textEditorContextMenuPopup.querySelector("[command=cmd_cut]");
  let cmdCopy = textEditorContextMenuPopup.querySelector("[command=cmd_copy]");
  let cmdPaste = textEditorContextMenuPopup.querySelector("[command=cmd_paste]");

  info ("Opening resource");
  let resource = projecteditor.project.allResources()[2];
  yield selectFile(projecteditor, resource);
  let editor = projecteditor.currentEditor;
  editor.editor.focus();

  info ("Opening context menu on resource");
  yield openContextMenuForEditor(editor, textEditorContextMenuPopup);

  is (cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is (cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is (cmdCut.getAttribute("disabled"), "true", "cmdCut is disabled");
  is (cmdCopy.getAttribute("disabled"), "true", "cmdCopy is disabled");
  is (cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  info ("Setting a selection and repening context menu on resource");
  yield closeContextMenuForEditor(editor, textEditorContextMenuPopup);
  editor.editor.setSelection({line: 0, ch: 0}, {line: 0, ch: 2});
  yield openContextMenuForEditor(editor, textEditorContextMenuPopup);

  is (cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is (cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is (cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is (cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is (cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");
});

function* openContextMenuForEditor(editor, contextMenu) {
  let editorDoc = editor.editor.container.contentDocument;
  let shown = onPopupShow(contextMenu);
  EventUtils.synthesizeMouse(editorDoc.body, 2, 2,
    {type: "contextmenu", button: 2}, editorDoc.defaultView);
  yield shown;
}
function* closeContextMenuForEditor(editor, contextMenu) {
  let editorDoc = editor.editor.container.contentDocument;
  let hidden = onPopupHidden(contextMenu);
  contextMenu.hidePopup();
  yield hidden;
}