/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model view continues to work after the page is reloaded

add_task(async function() {
  await addTab(URL_ROOT + "doc_boxmodel_iframe1.html");
  const {inspector, boxmodel, testActor} = await openLayoutView();

  info("Test that the box model view works on the first page");
  await assertBoxModelView(inspector, boxmodel, testActor);

  info("Reload the page");
  const onMarkupLoaded = waitForMarkupLoaded(inspector);
  await testActor.reload();
  await onMarkupLoaded;

  info("Test that the box model view works on the reloaded page");
  await assertBoxModelView(inspector, boxmodel, testActor);
});

async function assertBoxModelView(inspector, boxmodel, testActor) {
  await selectNode("p", inspector);

  info("Checking that the box model view shows the right value");
  const paddingElt = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-top > span");
  is(paddingElt.textContent, "50");

  info("Listening for box model view changes and modifying the padding");
  const onUpdated = waitForUpdate(inspector);
  await setStyle(testActor, "p", "padding", "20px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(paddingElt.textContent, "20");
}
