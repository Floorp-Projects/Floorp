/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model view continues to work after the page is reloaded

add_task(function* () {
  yield addTab(URL_ROOT + "doc_boxmodel_iframe1.html");
  let {inspector, view, testActor} = yield openBoxModelView();

  info("Test that the box model view works on the first page");
  yield assertBoxModelView(inspector, view, testActor);

  info("Reload the page");
  yield testActor.reload();
  yield inspector.once("markuploaded");

  info("Test that the box model view works on the reloaded page");
  yield assertBoxModelView(inspector, view, testActor);
});

function* assertBoxModelView(inspector, view, testActor) {
  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the box model view shows the right value");
  let paddingElt = view.doc.querySelector(".boxmodel-padding.boxmodel-top > span");
  is(paddingElt.textContent, "50");

  info("Listening for box model view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  yield setStyle(testActor, "p", "padding", "20px");
  yield onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(paddingElt.textContent, "20");
}
