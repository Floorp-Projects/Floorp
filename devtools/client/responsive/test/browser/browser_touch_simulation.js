/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global touch simulation button

const TEST_URL = `${URL_ROOT}touch.html`;
const PREF_DOM_META_VIEWPORT_ENABLED = "dom.meta-viewport.enabled";

addRDMTask(TEST_URL, async function({ ui }) {
  reloadOnTouchChange(true);

  await injectEventUtilsInContentTask(ui.getViewportBrowser());

  await waitBootstrap(ui);
  await testWithNoTouch(ui);
  await toggleTouchSimulation(ui);
  await testWithTouch(ui);
  await testWithMetaViewportEnabled(ui);
  await testWithMetaViewportDisabled(ui);
  testTouchButton(ui);

  reloadOnTouchChange(false);
});

async function testWithNoTouch(ui) {
  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
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
  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
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
    await EventUtils.synthesizeMouse(
      div,
      x,
      y,
      { type: "mousemove", isSynthesized: false },
      content
    );
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
}

async function testWithMetaViewportEnabled(ui) {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DOM_META_VIEWPORT_ENABLED, true]],
  });

  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    const { synthesizeClick } = EventUtils;

    const meta = content.document.querySelector("meta[name=viewport]");
    const div = content.document.querySelector("div");
    div.dataset.isDelay = "false";

    info(
      "testWithMetaViewportEnabled: " +
        "click the div element with <meta name='viewport'>"
    );
    meta.content = "";
    await synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "true",
      "300ms delay between touch events and mouse events should work"
    );

    info(
      "testWithMetaViewportEnabled: " +
        "click the div element with " +
        "<meta name='viewport' content='user-scalable=no'>"
    );
    meta.content = "user-scalable=no";
    await synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "false",
      "300ms delay between touch events and mouse events should not work"
    );

    info(
      "testWithMetaViewportEnabled: " +
        "click the div element with " +
        "<meta name='viewport' content='minimum-scale=1,maximum-scale=1'>"
    );
    meta.content = "minimum-scale=1,maximum-scale=1";
    await synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "false",
      "300ms delay between touch events and mouse events should not work"
    );

    info(
      "testWithMetaViewportEnabled: " +
        "click the div element with " +
        "<meta name='viewport' content='width=device-width'>"
    );
    meta.content = "width=device-width";
    await synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "false",
      "300ms delay between touch events and mouse events should not work"
    );
  });
}

async function testWithMetaViewportDisabled(ui) {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DOM_META_VIEWPORT_ENABLED, false]],
  });

  await ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    const { synthesizeClick } = EventUtils;

    const meta = content.document.querySelector("meta[name=viewport]");
    const div = content.document.querySelector("div");
    div.dataset.isDelay = "false";

    info(
      "testWithMetaViewportDisabled: click the div with <meta name='viewport'>"
    );
    meta.content = "";
    await synthesizeClick(div);
    is(
      div.dataset.isDelay,
      "true",
      "300ms delay between touch events and mouse events should work"
    );
  });
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
  const { store } = ui.toolWindow;

  await waitUntilState(store, state => state.viewports.length == 1);
  await waitForFrameLoad(ui, TEST_URL);
}
