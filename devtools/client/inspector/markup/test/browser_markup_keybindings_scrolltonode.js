/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the keyboard shortcut "S" used to scroll to the selected node.

const HTML =
  `<div style="width: 300px; height: 3000px; position:relative;">
    <div id="scroll-top"
      style="height: 50px; top: 0; position:absolute;">
      TOP</div>
    <div id="scroll-bottom"
      style="height: 50px; bottom: 0; position:absolute;">
      BOTTOM</div>
  </div>`;
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  info("Make sure the markup frame has the focus");
  inspector.markup._frame.focus();

  info("Before test starts, #scroll-top is visible, #scroll-bottom is hidden");
  await checkElementIsInViewport("#scroll-top", true, testActor);
  await checkElementIsInViewport("#scroll-bottom", false, testActor);

  info("Select the #scroll-bottom node");
  await selectNode("#scroll-bottom", inspector);
  info("Press S to scroll to the bottom node");
  let waitForScroll = testActor.waitForEventOnNode("scroll");
  await EventUtils.synthesizeKey("S", {}, inspector.panelWin);
  await waitForScroll;
  ok(true, "Scroll event received");

  info("#scroll-top should be scrolled out, #scroll-bottom should be visible");
  await checkElementIsInViewport("#scroll-top", false, testActor);
  await checkElementIsInViewport("#scroll-bottom", true, testActor);

  info("Select the #scroll-top node");
  await selectNode("#scroll-top", inspector);
  info("Press S to scroll to the top node");
  waitForScroll = testActor.waitForEventOnNode("scroll");
  await EventUtils.synthesizeKey("S", {}, inspector.panelWin);
  await waitForScroll;
  ok(true, "Scroll event received");

  info("#scroll-top should be visible, #scroll-bottom should be scrolled out");
  await checkElementIsInViewport("#scroll-top", true, testActor);
  await checkElementIsInViewport("#scroll-bottom", false, testActor);

  info("Select #scroll-bottom node");
  await selectNode("#scroll-bottom", inspector);
  info("Press shift + S, nothing should happen due to the modifier");
  await EventUtils.synthesizeKey("S", {shiftKey: true}, inspector.panelWin);

  info("Same state, #scroll-top is visible, #scroll-bottom is scrolled out");
  await checkElementIsInViewport("#scroll-top", true, testActor);
  await checkElementIsInViewport("#scroll-bottom", false, testActor);
});

/**
 * Verify that the element matching the provided selector is either in or out
 * of the viewport, depending on the provided "expected" argument.
 * Returns a promise that will resolve when the test has been performed.
 *
 * @param {String} selector
 *        css selector for the element to test
 * @param {Boolean} expected
 *        true if the element is expected to be in the viewport, false otherwise
 * @param {TestActor} testActor
 *        current test actor
 * @return {Promise} promise
 */
async function checkElementIsInViewport(selector, expected, testActor) {
  const isInViewport = await testActor.eval(`
    let node = document.querySelector("${selector}");
    let rect = node.getBoundingClientRect();
    rect.bottom >= 0 && rect.right >= 0 &&
      rect.top <= window.innerHeight && rect.left <= window.innerWidth;
  `);

  is(isInViewport, expected,
    selector + " in the viewport: expected to be " + expected);
}
