/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view for elements within iframes also updates when they
// change

add_task(function* () {
  yield addTab(URL_ROOT + "doc_layout_iframe1.html");
  let {inspector, view, testActor} = yield openLayoutView();

  yield testResizingInIframe(inspector, view, testActor);
  yield testReflowsAfterIframeDeletion(inspector, view, testActor);
});

function* testResizingInIframe(inspector, view, testActor) {
  info("Test that resizing an element in an iframe updates its box model");

  info("Selecting the nested test node");
  yield selectNodeInIframe2("div", inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".layout-size > span");
  is(sizeElt.textContent, "400\u00D7200");

  info("Listening for layout-view changes and modifying its size");
  let onUpdated = waitForUpdate(inspector);
  yield setStyleInIframe2(testActor, "div", "width", "200px");
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7200");
}

function* testReflowsAfterIframeDeletion(inspector, view, testActor) {
  info("Test reflows are still sent to the layout-view after deleting an " +
       "iframe");

  info("Deleting the iframe2");
  yield removeIframe2(testActor);
  yield inspector.once("inspector-updated");

  info("Selecting the test node in iframe1");
  yield selectNodeInIframe1("p", inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".layout-size > span");
  is(sizeElt.textContent, "100\u00D7100");

  info("Listening for layout-view changes and modifying its size");
  let onUpdated = waitForUpdate(inspector);
  yield setStyleInIframe1(testActor, "p", "width", "200px");
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7100");
}

function* selectNodeInIframe1(selector, inspector) {
  let iframe1 = yield getNodeFront("iframe", inspector);
  let node = yield getNodeFrontInFrame(selector, iframe1, inspector);
  yield selectNode(node, inspector);
}

function* selectNodeInIframe2(selector, inspector) {
  let iframe1 = yield getNodeFront("iframe", inspector);
  let iframe2 = yield getNodeFrontInFrame("iframe", iframe1, inspector);
  let node = yield getNodeFrontInFrame(selector, iframe2, inspector);
  yield selectNode(node, inspector);
}

function* setStyleInIframe1(testActor, selector, propertyName, value) {
  yield testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument.querySelector("${selector}")
           .style.${propertyName} = "${value}";
  `);
}

function* setStyleInIframe2(testActor, selector, propertyName, value) {
  yield testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument
           .querySelector("iframe")
           .contentDocument.querySelector("${selector}")
           .style.${propertyName} = "${value}";
  `);
}

function* removeIframe2(testActor) {
  yield testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument
           .querySelector("iframe")
           .remove();
  `);
}
