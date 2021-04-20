/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model view for elements within iframes also updates when they
// change

add_task(async function() {
  const tab = await addTab(URL_ROOT + "doc_boxmodel_iframe1.html");
  const browser = tab.linkedBrowser;
  const { inspector, boxmodel } = await openLayoutView();

  await testResizingInIframe(inspector, boxmodel, browser);
  await testReflowsAfterIframeDeletion(inspector, boxmodel, browser);
});

async function testResizingInIframe(inspector, boxmodel, browser) {
  info("Test that resizing an element in an iframe updates its box model");

  info("Selecting the nested test node");
  await selectNodeInFrames(["iframe", "iframe", "div"], inspector);

  info("Checking that the box model view shows the right value");
  const sizeElt = boxmodel.document.querySelector(".boxmodel-size > span");
  is(sizeElt.textContent, "400\u00D7200");

  info("Listening for box model view changes and modifying its size");
  const onUpdated = waitForUpdate(inspector);
  await setStyleInNestedIframe(browser, "div", "width", "200px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7200");
}

async function testReflowsAfterIframeDeletion(inspector, boxmodel, browser) {
  info(
    "Test reflows are still sent to the box model view after deleting an " +
      "iframe"
  );

  info("Deleting the iframe2");
  const onInspectorUpdated = inspector.once("inspector-updated");
  await removeNestedIframe(browser);
  await onInspectorUpdated;

  info("Selecting the test node in iframe1");
  await selectNodeInFrames(["iframe", "p"], inspector);

  info("Checking that the box model view shows the right value");
  const sizeElt = boxmodel.document.querySelector(".boxmodel-size > span");
  is(sizeElt.textContent, "100\u00D7100");

  info("Listening for box model view changes and modifying its size");
  const onUpdated = waitForUpdate(inspector);
  await setStyleInIframe(browser, "p", "width", "200px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7100");
}

async function setStyleInIframe(browser, selector, propertyName, value) {
  const context = await getBrowsingContextInFrames(browser, ["iframe"]);
  return setStyle(context, selector, propertyName, value);
}

async function setStyleInNestedIframe(browser, selector, propertyName, value) {
  const context = await getBrowsingContextInFrames(browser, [
    "iframe",
    "iframe",
  ]);
  return setStyle(context, selector, propertyName, value);
}

async function removeNestedIframe(browser) {
  const context = await getBrowsingContextInFrames(browser, ["iframe"]);
  await SpecialPowers.spawn(context, [], () =>
    content.document.querySelector("iframe").remove()
  );
}
