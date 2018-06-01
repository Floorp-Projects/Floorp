/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu items shown when clicking on linked attributes
// for <script> and <link> tags actually open the right tools.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

add_task(async function() {
  const {toolbox, inspector} = await openInspectorForURL(TEST_URL);

  info("Select a node with a cssresource attribute");
  await selectNode("link", inspector);

  info("Set the popupNode to the node that contains the uri");
  let {editor} = await getContainerForSelector("link", inspector);
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("href").querySelector(".link"),
  });

  info("Follow the link and wait for the style-editor to open");
  const onStyleEditorReady = toolbox.once("styleeditor-ready");
  inspector.onFollowLink();
  await onStyleEditorReady;

  // No real need to test that the editor opened on the right file here as this
  // is already tested in /framework/test/browser_toolbox_view_source_*
  ok(true, "The style-editor was open");

  info("Switch back to the inspector");
  await toolbox.selectTool("inspector");

  info("Select a node with a jsresource attribute");
  await selectNode("script", inspector);

  info("Set the popupNode to the node that contains the uri");
  ({editor} = await getContainerForSelector("script", inspector));
  openContextMenuAndGetAllItems(inspector, {
    target: editor.attrElements.get("src").querySelector(".link"),
  });

  info("Follow the link and wait for the debugger to open");
  const onDebuggerReady = toolbox.once("jsdebugger-ready");
  inspector.onFollowLink();
  await onDebuggerReady;

  // No real need to test that the debugger opened on the right file here as
  // this is already tested in /framework/test/browser_toolbox_view_source_*
  ok(true, "The debugger was open");
});
