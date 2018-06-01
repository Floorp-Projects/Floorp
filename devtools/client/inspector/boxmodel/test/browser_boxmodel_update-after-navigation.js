/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model view continues to work after a page navigation and that
// it also works after going back

const IFRAME1 = URL_ROOT + "doc_boxmodel_iframe1.html";
const IFRAME2 = URL_ROOT + "doc_boxmodel_iframe2.html";

add_task(async function() {
  await addTab(IFRAME1);
  const {inspector, boxmodel, testActor} = await openLayoutView();

  await testFirstPage(inspector, boxmodel, testActor);

  info("Navigate to the second page");
  let onMarkupLoaded = waitForMarkupLoaded(inspector);
  await testActor.eval(`location.href="${IFRAME2}"`);
  await onMarkupLoaded;

  await testSecondPage(inspector, boxmodel, testActor);

  info("Go back to the first page");
  onMarkupLoaded = waitForMarkupLoaded(inspector);
  await testActor.eval("history.back();");
  await onMarkupLoaded;

  await testBackToFirstPage(inspector, boxmodel, testActor);
});

async function testFirstPage(inspector, boxmodel, testActor) {
  info("Test that the box model view works on the first page");

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

async function testSecondPage(inspector, boxmodel, testActor) {
  info("Test that the box model view works on the second page");

  await selectNode("p", inspector);

  info("Checking that the box model view shows the right value");
  const sizeElt = boxmodel.document.querySelector(".boxmodel-size > span");
  is(sizeElt.textContent, "100" + "\u00D7" + "100");

  info("Listening for box model view changes and modifying the size");
  const onUpdated = waitForUpdate(inspector);
  await setStyle(testActor, "p", "width", "200px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(sizeElt.textContent, "200" + "\u00D7" + "100");
}

async function testBackToFirstPage(inspector, boxmodel, testActor) {
  info("Test that the box model view works on the first page after going back");

  await selectNode("p", inspector);

  info("Checking that the box model view shows the right value, which is the" +
    "modified value from step one because of the bfcache");
  const paddingElt = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-top > span");
  is(paddingElt.textContent, "20");

  info("Listening for box model view changes and modifying the padding");
  const onUpdated = waitForUpdate(inspector);
  await setStyle(testActor, "p", "padding", "100px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(paddingElt.textContent, "100");
}
