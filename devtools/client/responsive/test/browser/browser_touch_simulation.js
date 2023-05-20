/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global touch simulation button

const TEST_URL = `${URL_ROOT_SSL}touch.html`;
const PREF_DOM_META_VIEWPORT_ENABLED = "dom.meta-viewport.enabled";

// A 300ms delay between a `touchend` and `click` event is added whenever double-tap zoom
// is allowed.
const DELAY_MIN = 250;

addRDMTask(TEST_URL, async function ({ ui }) {
  reloadOnTouchChange(true);

  await waitBootstrap(ui);
  await testWithNoTouch(ui);
  await toggleTouchSimulation(ui);
  await promiseContentReflow(ui);
  await testWithTouch(ui);
  await testWithMetaViewportEnabled(ui);
  await testWithMetaViewportDisabled(ui);
  testTouchButton(ui);

  reloadOnTouchChange(false);
});

async function testWithNoTouch(ui) {
  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    const div = content.document.querySelector("div");
    let x = 0,
      y = 0;

    info("testWithNoTouch: Initial test parameter and mouse mouse outside div");
    x = -1;
    y = -1;
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    div.style.transform = "none";
    div.style.backgroundColor = "";

    info("testWithNoTouch: Move mouse into the div element");
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousemove", isSynthesized: false },
      content
    );
    is(div.style.backgroundColor, "red", "mouseenter or mouseover should work");

    info("testWithNoTouch: Drag the div element");
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousedown", isSynthesized: false },
      content
    );
    x = 100;
    y = 100;
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    is(div.style.transform, "none", "touchmove shouldn't work");
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mouseup", isSynthesized: false },
      content
    );

    info("testWithNoTouch: Move mouse out of the div element");
    x = -1;
    y = -1;
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    is(div.style.backgroundColor, "blue", "mouseout or mouseleave should work");

    info("testWithNoTouch: Click the div element");
    await EventUtils.synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "false",
      "300ms delay between touch events and mouse events should not work"
    );

    // Assuming that this test runs on devices having no touch screen device.
    ok(
      !content.document.defaultView.matchMedia("(pointer: coarse)").matches,
      "pointer: coarse shouldn't be matched"
    );
    ok(
      !content.document.defaultView.matchMedia("(hover: none)").matches,
      "hover: none shouldn't be matched"
    );
    ok(
      !content.document.defaultView.matchMedia("(any-pointer: coarse)").matches,
      "any-pointer: coarse shouldn't be matched"
    );
    ok(
      !content.document.defaultView.matchMedia("(any-hover: none)").matches,
      "any-hover: none shouldn't be matched"
    );
  });
}

async function testWithTouch(ui) {
  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    const div = content.document.querySelector("div");
    let x = 0,
      y = 0;

    info("testWithTouch: Initial test parameter and mouse mouse outside div");
    x = -1;
    y = -1;
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    div.style.transform = "none";
    div.style.backgroundColor = "";

    info("testWithTouch: Move mouse into the div element");
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousemove", isSynthesized: false },
      content
    );
    isnot(
      div.style.backgroundColor,
      "red",
      "mouseenter or mouseover should not work"
    );

    info("testWithTouch: Drag the div element");
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousedown", isSynthesized: false },
      content
    );
    x = 100;
    y = 100;
    const touchMovePromise = ContentTaskUtils.waitForEvent(div, "touchmove");
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    await touchMovePromise;
    isnot(div.style.transform, "none", "touchmove should work");
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mouseup", isSynthesized: false },
      content
    );

    info("testWithTouch: Move mouse out of the div element");
    x = -1;
    y = -1;
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
    isnot(
      div.style.backgroundColor,
      "blue",
      "mouseout or mouseleave should not work"
    );

    ok(
      content.document.defaultView.matchMedia("(pointer: coarse)").matches,
      "pointer: coarse should be matched"
    );
    ok(
      content.document.defaultView.matchMedia("(hover: none)").matches,
      "hover: none should be matched"
    );
    ok(
      content.document.defaultView.matchMedia("(any-pointer: coarse)").matches,
      "any-pointer: coarse should be matched"
    );
    ok(
      content.document.defaultView.matchMedia("(any-hover: none)").matches,
      "any-hover: none should be matched"
    );
  });

  // Capturing touch events with the content window as a registered listener causes the
  // "changedTouches" field to be undefined when using deprecated TouchEvent APIs.
  // See Bug 1549220 and Bug 1588438 for more information on this issue.
  info("Test that changed touches captured on the content window are defined.");
  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    const div = content.document.querySelector("div");

    content.addEventListener(
      "touchstart",
      event => {
        const changedTouch = event.changedTouches[0];
        ok(changedTouch, "Changed touch is defined.");
      },
      { once: true }
    );
    await EventUtils.synthesizeClick(div);
  });
}

async function testWithMetaViewportEnabled(ui) {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DOM_META_VIEWPORT_ENABLED, true]],
  });

  await SpecialPowers.spawn(
    ui.getViewportBrowser(),
    [{ delay_min: DELAY_MIN }],
    async function ({ delay_min }) {
      // A helper for testing the delay between touchend and click events.
      async function testDelay(mvc, el) {
        const touchendPromise = ContentTaskUtils.waitForEvent(el, "touchend");
        const clickPromise = ContentTaskUtils.waitForEvent(el, "click");
        await EventUtils.synthesizeClick(el);
        const { timeStamp: touchendTimestamp } = await touchendPromise;
        const { timeStamp: clickTimeStamp } = await clickPromise;
        const delay = clickTimeStamp - touchendTimestamp;

        const expected = delay >= delay_min;

        ok(
          expected,
          `${mvc}: There should be greater than a ${delay_min}ms delay between touch events and mouse events. Got delay of ${delay}ms`
        );
      }

      // A helper function for waiting for reflow to complete.
      const promiseReflow = () => {
        return new Promise(resolve => {
          content.window.requestAnimationFrame(() => {
            content.window.requestAnimationFrame(resolve);
          });
        });
      };

      const meta = content.document.querySelector("meta[name=viewport]");
      const div = content.document.querySelector("div");

      info(
        "testWithMetaViewportEnabled: " +
          "click the div element with <meta name='viewport'>"
      );
      meta.content = "";
      await promiseReflow();
      await testDelay("(empty)", div);
    }
  );

  await SpecialPowers.popPrefEnv();
}

async function testWithMetaViewportDisabled(ui) {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DOM_META_VIEWPORT_ENABLED, false]],
  });

  await SpecialPowers.spawn(
    ui.getViewportBrowser(),
    [{ delay_min: DELAY_MIN }],
    async function ({ delay_min }) {
      const meta = content.document.querySelector("meta[name=viewport]");
      const div = content.document.querySelector("div");

      info(
        "testWithMetaViewportDisabled: click the div with <meta name='viewport'>"
      );
      meta.content = "";
      const touchendPromise = ContentTaskUtils.waitForEvent(div, "touchend");
      const clickPromise = ContentTaskUtils.waitForEvent(div, "click");
      await EventUtils.synthesizeClick(div);
      const { timeStamp: touchendTimestamp } = await touchendPromise;
      const { timeStamp: clickTimeStamp } = await clickPromise;
      const delay = clickTimeStamp - touchendTimestamp;

      const expected = delay >= delay_min;

      ok(
        expected,
        `There should be greater than a ${delay_min}ms delay between touch events and mouse events. Got delay of ${delay}ms`
      );
    }
  );
}

function testTouchButton(ui) {
  const { document } = ui.toolWindow;
  const touchButton = document.getElementById("touch-simulation-button");

  ok(
    touchButton.classList.contains("checked"),
    "Touch simulation is active at end of test."
  );

  touchButton.click();

  ok(
    !touchButton.classList.contains("checked"),
    "Touch simulation is stopped on click."
  );

  touchButton.click();

  ok(
    touchButton.classList.contains("checked"),
    "Touch simulation is started on click."
  );
}

async function waitBootstrap(ui) {
  await waitForFrameLoad(ui, TEST_URL);
}
