/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL = "data:text/html;charset=utf-8,<body><div></div></body>";

add_task(async function() {
  const tab = await addTab(PAGE_URL);
  const toolbox = await openToolboxForTab(tab, "inspector", "bottom");
  const inspector = toolbox.getCurrentPanel();

  const root = await inspector.walker.getRootNode();
  const body = await inspector.walker.querySelector(root, "body");
  const node = await inspector.walker.querySelector(root, "div");

  is(inspector.selection.nodeFront, body, "Body is selected by default");

  // Listen to selection changed
  const onSelectionChanged = toolbox.once("selection-changed");

  info("Select the div and wait for the selection-changed event to be fired.");
  inspector.selection.setNodeFront(node, { reason: "browser-context-menu" });

  await onSelectionChanged;

  is(inspector.selection.nodeFront, node, "Div is now selected");

  // Listen to cleared selection changed
  const onClearSelectionChanged = toolbox.once("selection-changed");

  info(
    "Clear the selection and wait for the selection-changed event to be fired."
  );
  inspector.selection.setNodeFront(null);

  await onClearSelectionChanged;

  is(inspector.selection.nodeFront, null, "The selection is null as expected");
});
