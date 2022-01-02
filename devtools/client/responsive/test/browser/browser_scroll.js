/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test is checking that keyboard scrolling of content in RDM
 * behaves correctly, both with and without touch simulation enabled.
 */

const PAINT_LISTENER_JS_URL =
  URL_ROOT + "../../../../../../tests/SimpleTest/paint_listener.js";

const APZ_TEST_UTILS_JS_URL =
  URL_ROOT + "../../../../../gfx/layers/apz/test/mochitest/apz_test_utils.js";

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<head><meta name="viewport" content="width=100, height=100"/>' +
  '<script src="' +
  PAINT_LISTENER_JS_URL +
  '"></script>' +
  '<script src="' +
  APZ_TEST_UTILS_JS_URL +
  '"></script>' +
  "</head>" +
  '<div style="background:blue; width:200px; height:200px"></div>';

addRDMTask(TEST_URL, async function({ ui, manager }) {
  await setViewportSize(ui, manager, 50, 50);
  const browser = ui.getViewportBrowser();

  for (const mv in [true, false]) {
    await ui.updateTouchSimulation(mv);

    info("Setting focus on the browser.");
    browser.focus();

    await SpecialPowers.spawn(browser, [], async () => {
      // First of all, cancel any async scroll animation if there is. If there's
      // an on-going async scroll animation triggered by synthesizeKey, below
      // scrollTo call scrolls to a position nearby (0, 0) so that this test
      // won't work as expected.
      await content.wrappedJSObject.cancelScrollAnimation(
        content.document.scrollingElement,
        content
      );

      content.scrollTo(0, 0);
    });

    info("Testing scroll behavior with touch simulation " + mv + ".");
    await testScrollingOfContent(ui);
  }
});

async function testScrollingOfContent(ui) {
  let scroll;

  info("Checking initial scroll conditions.");
  const viewportScroll = await getViewportScroll(ui);
  is(viewportScroll.x, 0, "Content should load with scrollX 0.");
  is(viewportScroll.y, 0, "Content should load with scrollY 0.");

  /**
   * Here we're going to send off some arrow key events to trigger scrolling.
   * What we would like to be able to do is to await the scroll event and then
   * check the scroll position to confirm the amount of scrolling that has
   * happened. Unfortunately, APZ makes the scrolling happen asynchronously on
   * the compositor thread, and it's very difficult to await the end state of
   * the APZ animation -- see the tests in /gfx/layers/apz/test/mochitest for
   * an example. For our purposes, it's sufficient to test that the scroll
   * event is fired at all, and not worry about the amount of scrolling that
   * has occurred at the time of the event. If the key events don't trigger
   * scrolling, then no event will be fired and the test will time out.
   */
  scroll = waitForViewportScroll(ui);
  info("Synthesizing an arrow key down.");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await scroll;
  info("Scroll event was fired after arrow key down.");

  scroll = waitForViewportScroll(ui);
  info("Synthesizing an arrow key right.");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await scroll;
  info("Scroll event was fired after arrow key right.");
}
