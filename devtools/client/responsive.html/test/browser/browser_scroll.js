/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test is checking that keyboard scrolling of content in RDM
 * behaves correctly, both with and without touch simulation enabled.
 */

const TEST_URL = "data:text/html;charset=utf-8," +
  "<div style=\"background:blue; width:200px; height:200px\"></div>";

addRDMTask(TEST_URL, async function({ ui, manager }) {
  info("Turning off keyboard APZ for this test.");
  await SpecialPowers.pushPrefEnv({
    set: [["apz.keyboard.enabled", false]],
  });

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
  let x, y;
  let scroll;

  info("Checking initial scroll conditions.");
  ({x, y} = await getViewportScroll(ui));
  is(x, 0, "Content should load with scrollX 0.");
  is(y, 0, "Content should load with scrollY 0.");

  scroll = waitForViewportScroll(ui);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await scroll;
  ({x, y} = await getViewportScroll(ui));
  isnot(y, 0, "Down arrow key should scroll down by at least some amount.");

  scroll = waitForViewportScroll(ui);
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await scroll;
  ({x, y} = await getViewportScroll(ui));
  isnot(x, 0, "Right arrow key should scroll right by at least some amount.");
}
