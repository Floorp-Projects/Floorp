/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported testPopupSize */

// This file is imported into the same scope as head.js.

/* import-globals-from head.js */

// A test helper that retrives an old and new value after a given delay
// and then check that calls an `isCompleted` callback to check that
// the value has reached the expected value.
function waitUntilValue({
  getValue,
  isCompleted,
  message,
  delay: delayTime,
  times = 1,
} = {}) {
  let i = 0;
  return BrowserTestUtils.waitForCondition(async () => {
    const oldVal = await getValue();
    await delay(delayTime);
    const newVal = await getValue();

    const done = isCompleted(oldVal, newVal);

    // Reset the counter if the value wasn't the expected one.
    if (!done) {
      i = 0;
    }

    return done && times === ++i;
  }, message);
}

async function testPopupSize(
  standardsMode,
  browserWin = window,
  arrowSide = "top"
) {
  let docType = standardsMode ? "<!DOCTYPE html>" : "";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: false,
      },
    },

    files: {
      "popup.html": `${docType}
        <html>
          <head>
            <meta charset="utf-8">
            <style type="text/css">
              body > span {
                display: inline-block;
                width: 10px;
                height: 150px;
                border: 2px solid black;
              }
              .big > span {
                width: 300px;
                height: 100px;
              }
              .bigger > span {
                width: 150px;
                height: 150px;
              }
              .huge > span {
                height: ${2 * screen.height}px;
              }
            </style>
          </head>
          <body>
            <span></span>
            <span></span>
            <span></span>
            <span></span>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  if (arrowSide == "top") {
    // Test the standalone panel for a toolbar button.
    let browser = await openBrowserActionPanel(extension, browserWin, true);

    let dims = await promiseContentDimensions(browser);

    is(
      dims.isStandards,
      standardsMode,
      "Document has the expected compat mode"
    );

    let { innerWidth, innerHeight } = dims.window;

    dims = await alterContent(browser, () => {
      content.document.body.classList.add("bigger");
    });

    let win = dims.window;
    ok(
      Math.abs(win.innerHeight - innerHeight) <= 1,
      `Window height should not change (${win.innerHeight} ~= ${innerHeight})`
    );
    ok(
      win.innerWidth > innerWidth,
      `Window width should increase (${win.innerWidth} > ${innerWidth})`
    );

    dims = await alterContent(browser, () => {
      content.document.body.classList.remove("bigger");
    });

    win = dims.window;

    // The getContentSize calculation is not always reliable to single-pixel
    // precision.
    ok(
      Math.abs(win.innerHeight - innerHeight) <= 1,
      `Window height should return to approximately its original value (${win.innerHeight} ~= ${innerHeight})`
    );
    ok(
      Math.abs(win.innerWidth - innerWidth) <= 1,
      `Window width should return to approximately its original value (${win.innerWidth} ~= ${innerWidth})`
    );

    await closeBrowserAction(extension, browserWin);
  }

  // Test the PanelUI panel for a menu panel button.
  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, getCustomizableUIPanelID());

  let panel = browserWin.PanelUI.overflowPanel;
  panel.setAttribute("animate", "false");

  let panelMultiView = panel.firstElementChild;
  let widgetId = makeWidgetId(extension.id);
  // The 'ViewShown' event is the only way to correctly determine when the extensions'
  // panelview has finished transitioning and is fully in view.
  let shownPromise = BrowserTestUtils.waitForEvent(
    panelMultiView,
    "ViewShown",
    e => (e.originalTarget.id || "").includes(widgetId)
  );
  let browser = await openBrowserActionPanel(extension, browserWin);

  // Small changes if this is a fixed width window
  let isFixedWidth = !widget.disallowSubView;

  // Wait long enough to make sure the initial popup positioning has been completed (
  // by waiting until the value stays the same for 20 times in a row).
  await waitUntilValue({
    getValue: () => panel.getBoundingClientRect().top,
    isCompleted: (oldVal, newVal) => {
      return oldVal === newVal;
    },
    times: 20,
    message: "Wait the popup opening to be completed",
    delay: 500,
  });

  let origPanelRect = panel.getBoundingClientRect();

  // Check that the panel is still positioned as expected.
  let checkPanelPosition = () => {
    is(
      panel.getAttribute("side"),
      arrowSide,
      "Panel arrow is positioned as expected"
    );

    let panelRect = panel.getBoundingClientRect();
    if (arrowSide == "top") {
      is(panelRect.top, origPanelRect.top, "Panel has not moved downwards");
      ok(
        panelRect.bottom >= origPanelRect.bottom,
        `Panel has not shrunk from original size (${panelRect.bottom} >= ${origPanelRect.bottom})`
      );

      let screenBottom =
        browserWin.screen.availTop + browserWin.screen.availHeight;
      let panelBottom = browserWin.mozInnerScreenY + panelRect.bottom;
      ok(
        Math.round(panelBottom) <= screenBottom,
        `Bottom of popup should be on-screen. (${panelBottom} <= ${screenBottom})`
      );
    } else {
      is(panelRect.bottom, origPanelRect.bottom, "Panel has not moved upwards");
      ok(
        panelRect.top <= origPanelRect.top,
        `Panel has not shrunk from original size (${panelRect.top} <= ${origPanelRect.top})`
      );

      let panelTop = browserWin.mozInnerScreenY + panelRect.top;
      ok(
        panelTop >= browserWin.screen.availTop,
        `Top of popup should be on-screen. (${panelTop} >= ${browserWin.screen.availTop})`
      );
    }
  };

  await awaitBrowserLoaded(browser);
  await shownPromise;

  // Wait long enough to make sure the initial resize debouncing timer has
  // expired.
  await waitUntilValue({
    getValue: () => promiseContentDimensions(browser),
    isCompleted: (oldDims, newDims) => {
      return (
        oldDims.window.innerWidth === newDims.window.innerWidth &&
        oldDims.window.innerHeight === newDims.window.innerHeight
      );
    },
    message: "Wait the popup resize to be completed",
    delay: 500,
  });

  let dims = await promiseContentDimensions(browser);

  is(dims.isStandards, standardsMode, "Document has the expected compat mode");

  // If the browser's preferred height is smaller than the initial height of the
  // panel, then it will still take up the full available vertical space. Even
  // so, we need to check that we've gotten the preferred height calculation
  // correct, so check that explicitly.
  let getHeight = () => parseFloat(browser.style.height);

  let { innerWidth, innerHeight } = dims.window;
  let height = getHeight();

  let setClass = className => {
    content.document.body.className = className;
  };

  info(
    "Increase body children's width. " +
      "Expect them to wrap, and the frame to grow vertically rather than widen."
  );

  dims = await alterContent(browser, setClass, "big");
  let win = dims.window;

  ok(
    getHeight() > height,
    `Browser height should increase (${getHeight()} > ${height})`
  );

  if (isFixedWidth) {
    is(win.innerWidth, innerWidth, "Window width should not change");
  } else {
    ok(
      win.innerWidth >= innerWidth,
      `Window width should increase (${win.innerWidth} >= ${innerWidth})`
    );
  }
  ok(
    win.innerHeight >= innerHeight,
    `Window height should increase (${win.innerHeight} >= ${innerHeight})`
  );
  Assert.lessOrEqual(
    win.scrollMaxY,
    1,
    "Document should not be vertically scrollable"
  );

  checkPanelPosition();

  if (isFixedWidth) {
    // Test a fixed width window grows in height when elements wrap
    info(
      "Increase body children's width and height. " +
        "Expect them to wrap, and the frame to grow vertically rather than widen."
    );

    dims = await alterContent(browser, setClass, "bigger");
    win = dims.window;

    ok(
      getHeight() > height,
      `Browser height should increase (${getHeight()} > ${height})`
    );

    is(win.innerWidth, innerWidth, "Window width should not change");
    ok(
      win.innerHeight >= innerHeight,
      `Window height should increase (${win.innerHeight} >= ${innerHeight})`
    );
    Assert.lessOrEqual(
      win.scrollMaxY,
      1,
      "Document should not be vertically scrollable"
    );

    checkPanelPosition();
  }

  info(
    "Increase body height beyond the height of the screen. " +
      "Expect the panel to grow to accommodate, but not larger than the height of the screen."
  );

  dims = await alterContent(browser, setClass, "huge");
  win = dims.window;

  ok(
    getHeight() > height,
    `Browser height should increase (${getHeight()} > ${height})`
  );

  is(win.innerWidth, innerWidth, "Window width should not change");
  ok(
    win.innerHeight > innerHeight,
    `Window height should increase (${win.innerHeight} > ${innerHeight})`
  );
  // Commented out check for the window height here which mysteriously breaks
  // on infra but not locally. bug 1396843 covers re-enabling this.
  // ok(win.innerHeight < screen.height, `Window height be less than the screen height (${win.innerHeight} < ${screen.height})`);
  ok(
    win.scrollMaxY > 0,
    `Document should be vertically scrollable (${win.scrollMaxY} > 0)`
  );

  checkPanelPosition();

  info("Restore original styling. Expect original dimensions.");
  dims = await alterContent(browser, setClass, "");
  win = dims.window;

  is(getHeight(), height, "Browser height should return to its original value");

  is(win.innerWidth, innerWidth, "Window width should not change");
  is(
    win.innerHeight,
    innerHeight,
    "Window height should return to its original value"
  );
  Assert.lessOrEqual(
    win.scrollMaxY,
    1,
    "Document should not be vertically scrollable"
  );

  checkPanelPosition();

  await closeBrowserAction(extension, browserWin);

  await extension.unload();
}
