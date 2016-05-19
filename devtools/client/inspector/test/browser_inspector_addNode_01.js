/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the add node button and context menu items are present in the UI.

const TEST_URL = "data:text/html;charset=utf-8,<h1>Add node</h1>";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  let {panelDoc} = inspector;

  let toolbarButton =
    panelDoc.querySelector("#inspector-toolbar #inspector-element-add-button");
  let menuItem =
    panelDoc.querySelector("#inspector-node-popup #node-menu-add");

  ok(toolbarButton, "The add button is in the toolbar");
  ok(menuItem, "The item is in the menu");
});
