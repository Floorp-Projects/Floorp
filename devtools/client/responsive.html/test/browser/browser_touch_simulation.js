/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global touch simulation button

const TEST_URL = `${URL_ROOT}touch.html`;
const PREF_DOM_META_VIEWPORT_ENABLED = "dom.meta-viewport.enabled";

addRDMTask(TEST_URL, function* ({ ui }) {
  yield waitBootstrap(ui);
  yield testWithNoTouch(ui);
  yield enableTouchSimulation(ui);
  yield testWithTouch(ui);
  yield testWithMetaViewportEnabled(ui);
  yield testWithMetaViewportDisabled(ui);
  testTouchButton(ui);
});

function* testWithNoTouch(ui) {
  yield injectEventUtils(ui);
  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let { EventUtils } = content;

    let div = content.document.querySelector("div");
    let x = 0, y = 0;

    info("testWithNoTouch: Initial test parameter and mouse mouse outside div");
    x = -1; y = -1;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    div.style.transform = "none";
    div.style.backgroundColor = "";

    info("testWithNoTouch: Move mouse into the div element");
    yield EventUtils.synthesizeMouseAtCenter(div,
          { type: "mousemove", isSynthesized: false }, content);
    is(div.style.backgroundColor, "red", "mouseenter or mouseover should work");

    info("testWithNoTouch: Drag the div element");
    yield EventUtils.synthesizeMouseAtCenter(div,
          { type: "mousedown", isSynthesized: false }, content);
    x = 100; y = 100;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    is(div.style.transform, "none", "touchmove shouldn't work");
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mouseup", isSynthesized: false }, content);

    info("testWithNoTouch: Move mouse out of the div element");
    x = -1; y = -1;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    is(div.style.backgroundColor, "blue", "mouseout or mouseleave should work");

    info("testWithNoTouch: Click the div element");
    yield EventUtils.synthesizeClick(div);
    is(div.dataset.isDelay, "false",
      "300ms delay between touch events and mouse events should not work");
  });
}

function* testWithTouch(ui) {
  yield injectEventUtils(ui);

  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let { EventUtils } = content;

    let div = content.document.querySelector("div");
    let x = 0, y = 0;

    info("testWithTouch: Initial test parameter and mouse mouse outside div");
    x = -1; y = -1;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    div.style.transform = "none";
    div.style.backgroundColor = "";

    info("testWithTouch: Move mouse into the div element");
    yield EventUtils.synthesizeMouseAtCenter(div,
          { type: "mousemove", isSynthesized: false }, content);
    isnot(div.style.backgroundColor, "red",
      "mouseenter or mouseover should not work");

    info("testWithTouch: Drag the div element");
    yield EventUtils.synthesizeMouseAtCenter(div,
          { type: "mousedown", isSynthesized: false }, content);
    x = 100; y = 100;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    isnot(div.style.transform, "none", "touchmove should work");
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mouseup", isSynthesized: false }, content);

    info("testWithTouch: Move mouse out of the div element");
    x = -1; y = -1;
    yield EventUtils.synthesizeMouse(div, x, y,
          { type: "mousemove", isSynthesized: false }, content);
    isnot(div.style.backgroundColor, "blue",
      "mouseout or mouseleave should not work");
  });
}

function* testWithMetaViewportEnabled(ui) {
  yield SpecialPowers.pushPrefEnv({set: [[PREF_DOM_META_VIEWPORT_ENABLED, true]]});

  yield injectEventUtils(ui);

  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let { synthesizeClick } = content.EventUtils;

    let meta = content.document.querySelector("meta[name=viewport]");
    let div = content.document.querySelector("div");
    div.dataset.isDelay = "false";

    info("testWithMetaViewportEnabled: " +
         "click the div element with <meta name='viewport'>");
    meta.content = "";
    yield synthesizeClick(div);
    is(div.dataset.isDelay, "true",
      "300ms delay between touch events and mouse events should work");

    info("testWithMetaViewportEnabled: " +
         "click the div element with " +
         "<meta name='viewport' content='user-scalable=no'>");
    meta.content = "user-scalable=no";
    yield synthesizeClick(div);
    is(div.dataset.isDelay, "false",
      "300ms delay between touch events and mouse events should not work");

    info("testWithMetaViewportEnabled: " +
         "click the div element with " +
         "<meta name='viewport' content='minimum-scale=maximum-scale'>");
    meta.content = "minimum-scale=maximum-scale";
    yield synthesizeClick(div);
    is(div.dataset.isDelay, "false",
      "300ms delay between touch events and mouse events should not work");

    info("testWithMetaViewportEnabled: " +
         "click the div element with " +
         "<meta name='viewport' content='width=device-width'>");
    meta.content = "width=device-width";
    yield synthesizeClick(div);
    is(div.dataset.isDelay, "false",
      "300ms delay between touch events and mouse events should not work");
  });
}

function* testWithMetaViewportDisabled(ui) {
  yield SpecialPowers.pushPrefEnv({set: [[PREF_DOM_META_VIEWPORT_ENABLED, false]]});

  yield injectEventUtils(ui);

  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let { synthesizeClick } = content.EventUtils;

    let meta = content.document.querySelector("meta[name=viewport]");
    let div = content.document.querySelector("div");
    div.dataset.isDelay = "false";

    info("testWithMetaViewportDisabled: click the div with <meta name='viewport'>");
    meta.content = "";
    yield synthesizeClick(div);
    is(div.dataset.isDelay, "true",
      "300ms delay between touch events and mouse events should work");
  });
}

function testTouchButton(ui) {
  let { document } = ui.toolWindow;
  let touchButton = document.querySelector("#global-touch-simulation-button");

  ok(touchButton.classList.contains("active"),
    "Touch simulation is active at end of test.");

  touchButton.click();

  ok(!touchButton.classList.contains("active"),
    "Touch simulation is stopped on click.");

  touchButton.click();

  ok(touchButton.classList.contains("active"),
    "Touch simulation is started on click.");
}

function* waitBootstrap(ui) {
  let { store } = ui.toolWindow;

  yield waitUntilState(store, state => state.viewports.length == 1);
  yield waitForFrameLoad(ui, TEST_URL);
}

function* injectEventUtils(ui) {
  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    if ("EventUtils" in content) {
      return;
    }

    let EventUtils = content.EventUtils = {};

    EventUtils.window = {};
    EventUtils.parent = EventUtils.window;
    /* eslint-disable camelcase */
    EventUtils._EU_Ci = Components.interfaces;
    EventUtils._EU_Cc = Components.classes;
    /* eslint-enable camelcase */
    // EventUtils' `sendChar` function relies on the navigator to synthetize events.
    EventUtils.navigator = content.navigator;
    EventUtils.KeyboardEvent = content.KeyboardEvent;

    EventUtils.synthesizeClick = element => new Promise(resolve => {
      element.addEventListener("click", function onClick() {
        element.removeEventListener("click", onClick);
        resolve();
      });

      EventUtils.synthesizeMouseAtCenter(element,
        { type: "mousedown", isSynthesized: false }, content);
      EventUtils.synthesizeMouseAtCenter(element,
        { type: "mouseup", isSynthesized: false }, content);
    });

    Services.scriptloader.loadSubScript(
      "chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);
  });
}
