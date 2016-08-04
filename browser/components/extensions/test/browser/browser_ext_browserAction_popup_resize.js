/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* openPanel(extension, win = window) {
  clickBrowserAction(extension, win);

  let {target} = yield BrowserTestUtils.waitForEvent(win.document, "load", true, (event) => {
    return event.target.location && event.target.location.href.endsWith("popup.html");
  });

  return target.defaultView
               .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell)
               .chromeEventHandler;
}

function* awaitResize(browser) {
  // Debouncing code makes this a bit racy.
  // Try to skip the first, early resize, and catch the resize event we're
  // looking for, but don't wait longer than a few seconds.

  return Promise.race([
    BrowserTestUtils.waitForEvent(browser, "WebExtPopupResized")
      .then(() => BrowserTestUtils.waitForEvent(browser, "WebExtPopupResized")),
    new Promise(resolve => setTimeout(resolve, 5000)),
  ]);
}

add_task(function* testBrowserActionPopupResize() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": '<html><head><meta charset="utf-8"></head></html>',
    },
  });

  yield extension.startup();

  clickBrowserAction(extension, window);

  let browser = yield openPanel(extension);
  let panelWindow = browser.contentWindow;
  let panelBody = panelWindow.document.body;

  function checkSize(expected) {
    is(panelWindow.innerHeight, expected, `Panel window should be ${expected}px tall`);
    is(panelBody.clientHeight, panelBody.scrollHeight,
      "Panel body should be tall enough to fit its contents");

    // Tolerate if it is 1px too wide, as that may happen with the current resizing method.
    ok(Math.abs(panelWindow.innerWidth - expected) <= 1, `Panel window should be ${expected}px wide`);
    is(panelBody.clientWidth, panelBody.scrollWidth,
      "Panel body should be wide enough to fit its contents");
  }

  function setSize(size) {
    panelBody.style.height = `${size}px`;
    panelBody.style.width = `${size}px`;
  }

  let sizes = [
    200,
    400,
    300,
  ];

  for (let size of sizes) {
    setSize(size);
    yield awaitResize(browser);
    checkSize(size);
  }

  yield closeBrowserAction(extension);
  yield extension.unload();
});

function* testPopupSize(standardsMode, browserWin = window, arrowSide = "top") {
  let docType = standardsMode ? "<!DOCTYPE html>" : "";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": false,
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

  yield extension.startup();

  if (arrowSide == "top") {
    // Test the standalone panel for a toolbar button.
    let browser = yield openPanel(extension, browserWin);
    let win = browser.contentWindow;
    let body = win.document.body;

    let isStandards = win.document.compatMode != "BackCompat";
    is(isStandards, standardsMode, "Document has the expected compat mode");

    let {innerWidth, innerHeight} = win;

    body.classList.add("bigger");
    yield awaitResize(browser);

    is(win.innerHeight, innerHeight, "Window height should not change");
    ok(win.innerWidth > innerWidth, `Window width should increase (${win.innerWidth} > ${innerWidth})`);


    body.classList.remove("bigger");
    yield awaitResize(browser);

    is(win.innerHeight, innerHeight, "Window height should not change");

    // The getContentSize calculation is not always reliable to single-pixel
    // precision.
    ok(Math.abs(win.innerWidth - innerWidth) <= 1,
       `Window width should return to approximately its original value (${win.innerWidth} ~= ${innerWidth})`);

    yield closeBrowserAction(extension, browserWin);
  }


  // Test the PanelUI panel for a menu panel button.
  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, CustomizableUI.AREA_PANEL);

  let browser = yield openPanel(extension, browserWin);
  let win = browser.contentWindow;
  let body = win.document.body;

  let {panel} = browserWin.PanelUI;
  let origPanelRect = panel.getBoundingClientRect();

  // Check that the panel is still positioned as expected.
  let checkPanelPosition = () => {
    is(panel.getAttribute("side"), arrowSide, "Panel arrow is positioned as expected");

    function isGreaterThanOrWithinPixelRoundingError(a, b, message) {
      let result = a + 1 >= b;
      ok(result, `${a} should be greater than or within one pixel of ${b}: ${message}`);
    }

    let panelRect = panel.getBoundingClientRect();
    if (arrowSide == "top") {
      isGreaterThanOrWithinPixelRoundingError(panelRect.top, origPanelRect.top, "Panel has not moved downwards");
      isGreaterThanOrWithinPixelRoundingError(panelRect.bottom, origPanelRect.bottom, "Panel has not shrunk from original size");

      let screenBottom = browserWin.screen.availTop + win.screen.availHeight;
      let panelBottom = browserWin.mozInnerScreenY + panelRect.bottom;
      ok(panelBottom <= screenBottom, `Bottom of popup should be on-screen. (${panelBottom} <= ${screenBottom})`);
    } else {
      isGreaterThanOrWithinPixelRoundingError(panelRect.bottom, origPanelRect.bottom, "Panel has not moved upwards");
      // The arguments here are reversed compared to the above calls due to the coordinate system.
      isGreaterThanOrWithinPixelRoundingError(origPanelRect.top, panelRect.top, "Panel has not shrunk from original size");

      let panelTop = browserWin.mozInnerScreenY + panelRect.top;
      ok(panelTop >= browserWin.screen.availTop, `Top of popup should be on-screen. (${panelTop} >= ${browserWin.screen.availTop})`);
    }
  };


  let isStandards = win.document.compatMode != "BackCompat";
  is(isStandards, standardsMode, "Document has the expected compat mode");

  // Wait long enough to make sure the initial resize debouncing timer has
  // expired.
  yield new Promise(resolve => setTimeout(resolve, 100));

  // If the browser's preferred height is smaller than the initial height of the
  // panel, then it will still take up the full available vertical space. Even
  // so, we need to check that we've gotten the preferred height calculation
  // correct, so check that explicitly.
  let getHeight = () => parseFloat(browser.style.height);

  let {innerWidth, innerHeight} = win;
  let height = getHeight();


  info("Increase body children's width. " +
       "Expect them to wrap, and the frame to grow vertically rather than widen.");
  body.className = "big";
  yield awaitResize(browser);

  ok(getHeight() > height, `Browser height should increase (${getHeight()} > ${height})`);

  is(win.innerWidth, innerWidth, "Window width should not change");
  ok(win.innerHeight >= innerHeight, `Window height should increase (${win.innerHeight} >= ${innerHeight})`);
  is(win.scrollMaxY, 0, "Document should not be vertically scrollable");

  checkPanelPosition();


  info("Increase body children's width and height. " +
       "Expect them to wrap, and the frame to grow vertically rather than widen.");
  body.className = "bigger";
  yield awaitResize(browser);

  ok(getHeight() > height, `Browser height should increase (${getHeight()} > ${height})`);

  is(win.innerWidth, innerWidth, "Window width should not change");
  ok(win.innerHeight >= innerHeight, `Window height should increase (${win.innerHeight} >= ${innerHeight})`);
  is(win.scrollMaxY, 0, "Document should not be vertically scrollable");

  checkPanelPosition();


  info("Increase body height beyond the height of the screen. " +
       "Expect the panel to grow to accommodate, but not larger than the height of the screen.");
  body.className = "huge";
  yield awaitResize(browser);

  ok(getHeight() > height, `Browser height should increase (${getHeight()} > ${height})`);

  is(win.innerWidth, innerWidth, "Window width should not change");
  ok(win.innerHeight > innerHeight, `Window height should increase (${win.innerHeight} > ${innerHeight})`);
  ok(win.innerHeight < screen.height, `Window height be less than the screen height (${win.innerHeight} < ${screen.height})`);
  ok(win.scrollMaxY > 0, `Document should be vertically scrollable (${win.scrollMaxY} > 0)`);

  checkPanelPosition();


  info("Restore original styling. Expect original dimensions.");
  body.className = "";
  yield awaitResize(browser);

  is(getHeight(), height, "Browser height should return to its original value");

  is(win.innerWidth, innerWidth, "Window width should not change");
  is(win.innerHeight, innerHeight, "Window height should return to its original value");
  is(win.scrollMaxY, 0, "Document should not be vertically scrollable");

  checkPanelPosition();

  yield closeBrowserAction(extension, browserWin);

  yield extension.unload();
}

add_task(function* testBrowserActionMenuResizeStandards() {
  yield testPopupSize(true);
});

add_task(function* testBrowserActionMenuResizeQuirks() {
  yield testPopupSize(false);
});

// Test that we still make reasonable maximum size calculations when the window
// is close enough to the bottom of the screen that the menu panel opens above,
// rather than below, its button.
add_task(function* testBrowserActionMenuResizeBottomArrow() {
  const WIDTH = 800;
  const HEIGHT = 300;

  let left = screen.availLeft + screen.availWidth - WIDTH;
  let top = screen.availTop + screen.availHeight - HEIGHT;

  let win = yield BrowserTestUtils.openNewBrowserWindow();

  win.resizeTo(WIDTH, HEIGHT);

  // Sometimes we run into problems on Linux with resizing being asynchronous
  // and window managers not allowing us to move the window so that any part of
  // it is off-screen, so we need to try more than once.
  for (let i = 0; i < 20; i++) {
    win.moveTo(left, top);

    if (win.screenX == left && win.screenY == top) {
      break;
    }

    yield new Promise(resolve => setTimeout(resolve, 100));
  }

  yield testPopupSize(true, win, "bottom");

  yield BrowserTestUtils.closeWindow(win);
});
