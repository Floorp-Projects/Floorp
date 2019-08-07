/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test is checking that keyboard scrolling of content in RDM
 * behaves correctly, both with and without touch simulation enabled.
 */

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<div style="background:blue; width:200px; height:200px"></div>';

addRDMTask(TEST_URL, async function({ ui, manager }) {
  await setViewportSize(ui, manager, 100, 100);
  const browser = ui.getViewportBrowser();

  info("Setting focus on the browser.");
  browser.focus();

  info("Testing scroll behavior with touch simulation OFF.");
  await testScrollingOfContent(ui);

  // Run the tests again with touch simulation on.
  const reloadNeeded = await ui.updateTouchSimulation(true);
  if (reloadNeeded) {
    info("Reload is needed -- waiting for it.");
    const reload = waitForViewportLoad(ui);
    browser.reload();
    await reload;

    await ContentTask.spawn(browser, null, () => {
      content.scrollTo(0, 0);
    });
  }

  info("Testing scroll behavior with touch simulation ON.");
  await testScrollingOfContent(ui);
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
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await scroll;
  info("Scroll event was fired after arrow key down.");

  scroll = waitForViewportScroll(ui);
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await scroll;
  info("Scroll event was fired after arrow key right.");
}
