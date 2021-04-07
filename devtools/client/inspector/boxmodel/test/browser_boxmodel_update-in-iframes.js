/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model view for elements within iframes also updates when they
// change

add_task(async function() {
  await addTab(URL_ROOT + "doc_boxmodel_iframe1.html");
  const { inspector, boxmodel, testActor } = await openLayoutView();

  await testResizingInIframe(inspector, boxmodel, testActor);
  await testReflowsAfterIframeDeletion(inspector, boxmodel, testActor);
});

async function testResizingInIframe(inspector, boxmodel, testActor) {
  info("Test that resizing an element in an iframe updates its box model");

  info("Selecting the nested test node");
  await selectNodeInFrames(["iframe", "iframe", "div"], inspector);

  info("Checking that the box model view shows the right value");
  const sizeElt = boxmodel.document.querySelector(".boxmodel-size > span");
  is(sizeElt.textContent, "400\u00D7200");

  info("Listening for box model view changes and modifying its size");
  const onUpdated = waitForUpdate(inspector);
  await setStyleInIframe2(testActor, "div", "width", "200px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7200");
}

async function testReflowsAfterIframeDeletion(inspector, boxmodel, testActor) {
  info(
    "Test reflows are still sent to the box model view after deleting an " +
      "iframe"
  );

  info("Deleting the iframe2");
  const onInspectorUpdated = inspector.once("inspector-updated");
  await removeIframe2(testActor);
  await onInspectorUpdated;

  info("Selecting the test node in iframe1");
  await selectNodeInFrames(["iframe", "p"], inspector);

  info("Checking that the box model view shows the right value");
  const sizeElt = boxmodel.document.querySelector(".boxmodel-size > span");
  is(sizeElt.textContent, "100\u00D7100");

  info("Listening for box model view changes and modifying its size");
  const onUpdated = waitForUpdate(inspector);
  await setStyleInIframe1(testActor, "p", "width", "200px");
  await onUpdated;
  ok(true, "Box model view got updated");

  info("Checking that the box model view shows the right value after update");
  is(sizeElt.textContent, "200\u00D7100");
}

async function setStyleInIframe1(testActor, selector, propertyName, value) {
  await testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument.querySelector("${selector}")
           .style.${propertyName} = "${value}";
  `);
}

async function setStyleInIframe2(testActor, selector, propertyName, value) {
  await testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument
           .querySelector("iframe")
           .contentDocument.querySelector("${selector}")
           .style.${propertyName} = "${value}";
  `);
}

async function removeIframe2(testActor) {
  await testActor.eval(`
    content.document.querySelector("iframe")
           .contentDocument
           .querySelector("iframe")
           .remove();
  `);
}
