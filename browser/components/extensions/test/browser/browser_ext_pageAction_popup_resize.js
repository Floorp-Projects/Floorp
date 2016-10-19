/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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

add_task(function* testPageActionPopupResize() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },
    background: function() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        const tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("action-shown");
        });
      });
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><div></div></body></html>`,
    },
  });

  yield extension.startup();
  yield extension.awaitMessage("action-shown");

  clickPageAction(extension, window);

  let {target: panelDocument} = yield BrowserTestUtils.waitForEvent(document, "load", true, (event) => {
    info(`Loaded ${event.target.location}`);
    return event.target.location && event.target.location.href.endsWith("popup.html");
  });

  let panelWindow = panelDocument.defaultView;
  let panelBody = panelDocument.body.firstChild;
  let body = panelDocument.body;
  let root = panelDocument.documentElement;

  function checkSize(expected) {
    is(panelWindow.innerHeight, expected, `Panel window should be ${expected}px tall`);
    is(body.clientHeight, body.scrollHeight,
      "Panel body should be tall enough to fit its contents");
    is(root.clientHeight, root.scrollHeight,
      "Panel root should be tall enough to fit its contents");

    // Tolerate if it is 1px too wide, as that may happen with the current resizing method.
    ok(Math.abs(panelWindow.innerWidth - expected) <= 1, `Panel window should be ${expected}px wide`);
    is(body.clientWidth, body.scrollWidth,
       "Panel body should be wide enough to fit its contents");
  }

  function setSize(size) {
    panelBody.style.height = `${size}px`;
    panelBody.style.width = `${size}px`;

    return BrowserTestUtils.waitForEvent(panelWindow, "resize");
  }

  let sizes = [
    200,
    400,
    300,
  ];

  for (let size of sizes) {
    yield setSize(size);
    checkSize(size);
  }

  yield setSize(1400);

  is(panelWindow.innerWidth, 800, "Panel window width");
  ok(body.clientWidth <= 800, "Panel body width ${body.clientWidth} is less than 800");
  is(body.scrollWidth, 1400, "Panel body scroll width");

  is(panelWindow.innerHeight, 600, "Panel window height");
  ok(root.clientHeight <= 600, `Panel root height (${root.clientHeight}px) is less than 600px`);
  is(root.scrollHeight, 1400, "Panel root scroll height");

  yield extension.unload();
});

add_task(function* testPageActionPopupReflow() {
  let browser;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },
    background: function() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
        const tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("action-shown");
        });
      });
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head>
        <body>
          The quick mauve fox jumps over the opalescent toad, with its glowing
          eyes, and its vantablack mouth, and its bottomless chasm where you
          would hope to find a heart, that looks straight into the deepest
          pits of hell. The fox shivers, and cowers, and tries to run, but
          the toad is utterly without pity. It turns, ever so slightly...
        </body>
      </html>`,
    },
  });

  yield extension.startup();
  yield extension.awaitMessage("action-shown");

  clickPageAction(extension, window);

  browser = yield awaitExtensionPanel(extension);

  let win = browser.contentWindow;
  let body = win.document.body;
  let root = win.document.documentElement;

  function setSize(size) {
    body.style.fontSize = `${size}px`;

    return awaitResize(browser);
  }

  yield setSize(18);

  is(win.innerWidth, 800, "Panel window should be 800px wide");
  is(body.clientWidth, 800, "Panel body should be 800px wide");
  is(body.clientWidth, body.scrollWidth,
     "Panel body should be wide enough to fit its contents");

  ok(win.innerHeight > 36,
     `Panel window height (${win.innerHeight}px) should be taller than two lines of text.`);

  is(body.clientHeight, body.scrollHeight,
    "Panel body should be tall enough to fit its contents");
  is(root.clientHeight, root.scrollHeight,
    "Panel root should be tall enough to fit its contents");

  yield extension.unload();
});
