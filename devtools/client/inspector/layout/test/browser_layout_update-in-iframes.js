/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view for elements within iframes also updates when they
// change

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_layout_iframe1.html");
  let iframe2 = getNode("iframe").contentDocument.querySelector("iframe");

  let {toolbox, inspector, view} = yield openLayoutView();
  yield runTests(inspector, view, iframe2);
});

addTest("Test that resizing an element in an iframe updates its box model",
function*(inspector, view, iframe2) {
  info("Selecting the nested test node");
  let node = iframe2.contentDocument.querySelector("div");
  yield selectNode(node, inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".size > span");
  is(sizeElt.textContent, "400x200");

  info("Listening for layout-view changes and modifying its size");
  let onUpdated = waitForUpdate(inspector);
  node.style.width = "200px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200x200");
});

addTest("Test reflows are still sent to the layout-view after deleting an iframe",
function*(inspector, view, iframe2) {
  info("Deleting the iframe2");
  iframe2.remove();
  yield inspector.once("inspector-updated");

  info("Selecting the test node in iframe1");
  let node = getNode("iframe").contentDocument.querySelector("p");
  yield selectNode(node, inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".size > span");
  is(sizeElt.textContent, "100x100");

  info("Listening for layout-view changes and modifying its size");
  let onUpdated = waitForUpdate(inspector);
  node.style.width = "200px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200x100");
});
