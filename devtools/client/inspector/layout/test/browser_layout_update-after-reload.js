/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view continues to work after the page is reloaded

add_task(function* () {
  yield addTab(URL_ROOT + "doc_layout_iframe1.html");
  let {inspector, view, testActor} = yield openLayoutView();

  info("Test that the layout-view works on the first page");
  yield assertLayoutView(inspector, view, testActor);

  info("Reload the page");
  yield testActor.reload();
  yield inspector.once("markuploaded");

  info("Test that the layout-view works on the reloaded page");
  yield assertLayoutView(inspector, view, testActor);
});

function* assertLayoutView(inspector, view, testActor) {
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
