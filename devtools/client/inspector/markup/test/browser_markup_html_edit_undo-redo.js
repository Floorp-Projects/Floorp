/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the undo/redo stack is correctly cleared when opening the HTML editor on a
// new node. Bug 1327674.

const DIV1_HTML = "<div id=\"d1\">content1</div>";
const DIV2_HTML = "<div id=\"d2\">content2</div>";
const DIV2_HTML_UPDATED = "<div id=\"d2\">content2_updated</div>";

const TEST_URL = "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
    DIV1_HTML +
    DIV2_HTML +
  "</body>" +
  "</html>";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  inspector.markup._frame.focus();

  await selectNode("#d1", inspector);

  info("Open the HTML editor on node #d1");
  let onHtmlEditorCreated = once(inspector.markup, "begin-editing");
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  await onHtmlEditorCreated;

  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");
  is(inspector.markup.htmlEditor.editor.getText(), DIV1_HTML,
      "The editor content for d1 is correct.");

  info("Hide the HTML editor for #d1");
  let onEditorHidden = once(inspector.markup.htmlEditor, "popuphidden");
  EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
  await onEditorHidden;
  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");

  await selectNode("#d2", inspector);

  info("Open the HTML editor on node #d2");
  onHtmlEditorCreated = once(inspector.markup, "begin-editing");
  EventUtils.sendKey("F2", inspector.markup._frame.contentWindow);
  await onHtmlEditorCreated;

  ok(inspector.markup.htmlEditor._visible, "HTML Editor is visible");
  is(inspector.markup.htmlEditor.editor.getText(), DIV2_HTML,
      "The editor content for d2 is correct.");

  inspector.markup.htmlEditor.editor.setText(DIV2_HTML_UPDATED);
  is(inspector.markup.htmlEditor.editor.getText(), DIV2_HTML_UPDATED,
      "The editor content for d2 is updated.");

  inspector.markup.htmlEditor.editor.undo();
  is(inspector.markup.htmlEditor.editor.getText(), DIV2_HTML,
      "The editor content for d2 is reverted.");

  inspector.markup.htmlEditor.editor.undo();
  is(inspector.markup.htmlEditor.editor.getText(), DIV2_HTML,
      "The editor content for d2 has not been set to content1.");

  info("Hide the HTML editor for #d2");
  onEditorHidden = once(inspector.markup.htmlEditor, "popuphidden");
  EventUtils.sendKey("ESCAPE", inspector.markup.htmlEditor.doc.defaultView);
  await onEditorHidden;
  ok(!inspector.markup.htmlEditor._visible, "HTML Editor is not visible");
});
