/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that markup view handles page navigation correctly.

const URL_1 = URL_ROOT + "doc_markup_update-on-navigtion_1.html";
const URL_2 = URL_ROOT + "doc_markup_update-on-navigtion_2.html";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(URL_1);

  assertMarkupViewIsLoaded();
  yield selectNode("#one", inspector);

  let willNavigate = inspector.target.once("will-navigate");
  yield testActor.eval(`content.location = "${URL_2}"`);

  info("Waiting for will-navigate");
  yield willNavigate;

  info("Navigation to page 2 has started, the inspector should be empty");
  assertMarkupViewIsEmpty();

  info("Waiting for new-root");
  yield inspector.once("new-root");

  info("Navigation to page 2 was done, the inspector should be back up");
  assertMarkupViewIsLoaded();

  yield selectNode("#two", inspector);

  function assertMarkupViewIsLoaded() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 1, "The markup-view is loaded");
  }

  function assertMarkupViewIsEmpty() {
    let markupViewBox = inspector.panelDoc.getElementById("markup-box");
    is(markupViewBox.childNodes.length, 0, "The markup-view is unloaded");
  }
});
