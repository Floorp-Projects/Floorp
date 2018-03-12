/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL = "data:text/html;charset=utf-8,<body><div></div></body>";

add_task(function* () {
  let tab = yield addTab(PAGE_URL);
  let toolbox = yield openToolboxForTab(tab, "inspector", "bottom");
  let inspector = toolbox.getCurrentPanel();

  let root = yield inspector.walker.getRootNode();
  let body = yield inspector.walker.querySelector(root, "body");
  let node = yield inspector.walker.querySelector(root, "div");

  is(inspector.selection.nodeFront, body, "Body is selected by default");

  // Listen to selection changed
  let onSelectionChanged = toolbox.once("selection-changed");

  info("Select the div and wait for the selection-changed event to be fired.");
  inspector.selection.setNodeFront(node, "browser-context-menu");

  yield onSelectionChanged;

  is(inspector.selection.nodeFront, node, "Div is now selected");

  // Listen to cleared selection changed
  let onClearSelectionChanged = toolbox.once("selection-changed");

  info("Clear the selection and wait for the selection-changed event to be fired.");
  inspector.selection.setNodeFront(undefined, "browser-context-menu");

  yield onClearSelectionChanged;

  is(inspector.selection.nodeFront, undefined, "The selection is undefined as expected");
});

