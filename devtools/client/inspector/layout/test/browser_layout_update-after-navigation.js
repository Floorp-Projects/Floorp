/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view continues to work after a page navigation and that
// it also works after going back

const IFRAME1 = URL_ROOT + "doc_layout_iframe1.html";
const IFRAME2 = URL_ROOT + "doc_layout_iframe2.html";

add_task(function* () {
  yield addTab(IFRAME1);
  let {inspector, view, testActor} = yield openLayoutView();

  yield testFirstPage(inspector, view, testActor);

  info("Navigate to the second page");
  yield testActor.eval(`content.location.href="${IFRAME2}"`);
  yield inspector.once("markuploaded");

  yield testSecondPage(inspector, view, testActor);

  info("Go back to the first page");
  yield testActor.eval("content.history.back();");
  yield inspector.once("markuploaded");

  yield testBackToFirstPage(inspector, view, testActor);
});

function* testFirstPage(inspector, view, testActor) {
  info("Test that the layout-view works on the first page");

  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value");
  let paddingElt = view.doc.querySelector(".layout-padding.layout-top > span");
  is(paddingElt.textContent, "50");

  info("Listening for layout-view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  yield setStyle(testActor, "p", "padding", "20px");
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(paddingElt.textContent, "20");
}

function* testSecondPage(inspector, view, testActor) {
  info("Test that the layout-view works on the second page");

  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value");
  let sizeElt = view.doc.querySelector(".layout-size > span");
  is(sizeElt.textContent, "100" + "\u00D7" + "100");

  info("Listening for layout-view changes and modifying the size");
  let onUpdated = waitForUpdate(inspector);
  yield setStyle(testActor, "p", "width", "200px");
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(sizeElt.textContent, "200" + "\u00D7" + "100");
}

function* testBackToFirstPage(inspector, view, testActor) {
  info("Test that the layout-view works on the first page after going back");

  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value, which is the" +
    "modified value from step one because of the bfcache");
  let paddingElt = view.doc.querySelector(".layout-padding.layout-top > span");
  is(paddingElt.textContent, "20");

  info("Listening for layout-view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  yield setStyle(testActor, "p", "padding", "100px");
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(paddingElt.textContent, "100");
}
