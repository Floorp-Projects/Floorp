/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the inspector node context menu appears when right-clicking on the
// breadcrumbs nodes.

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs.html";

add_task(function*() {
  let {inspector} = yield openInspectorForURL(TEST_URI);
  let container = inspector.panelDoc.getElementById("inspector-breadcrumbs");

  info("Select a test node and try to right-click on the selected breadcrumb");
  yield selectNode("#i1", inspector);
  let button = container.querySelector("button[checked]");

  let onMenuShown = once(inspector.nodemenu, "popupshown");
  button.onclick({button: 2});
  yield onMenuShown;

  ok(true, "The context menu appeared on right-click");

  info("Right-click on a non selected crumb (the body node)");
  button = button.previousSibling;
  onMenuShown = once(inspector.nodemenu, "popupshown");
  let onInspectorUpdated = inspector.once("inspector-updated");
  button.onclick({button: 2});

  yield onMenuShown;
  ok(true, "The context menu appeared on right-click");

  yield onInspectorUpdated;
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "body",
     "The body node was selected when right-clicking in the breadcrumbs");

  info("Right-click on the html node");
  button = button.previousSibling;
  onMenuShown = once(inspector.nodemenu, "popupshown");
  onInspectorUpdated = inspector.once("inspector-updated");
  button.onclick({button: 2});

  yield onMenuShown;
  ok(true, "The context menu appeared on right-click");

  yield onInspectorUpdated;
  is(inspector.selection.nodeFront.tagName.toLowerCase(), "html",
     "The html node was selected when right-clicking in the breadcrumbs");
});
