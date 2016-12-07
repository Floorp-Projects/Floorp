/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testPageActionPopupResize() {
  let browser;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },
    background: function() {
      /* global browser */
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

  browser = yield awaitExtensionPanel(extension);

  function* checkSize(expected) {
    let dims = yield promiseContentDimensions(browser);
    let {body, root} = dims;

    is(dims.window.innerHeight, expected, `Panel window should be ${expected}px tall`);
    is(body.clientHeight, body.scrollHeight,
      "Panel body should be tall enough to fit its contents");
    is(root.clientHeight, root.scrollHeight,
      "Panel root should be tall enough to fit its contents");

    // Tolerate if it is 1px too wide, as that may happen with the current resizing method.
    ok(Math.abs(dims.window.innerWidth - expected) <= 1, `Panel window should be ${expected}px wide`);
    is(body.clientWidth, body.scrollWidth,
       "Panel body should be wide enough to fit its contents");
  }

  /* eslint-disable mozilla/no-cpows-in-tests */
  function setSize(size) {
    let elem = content.document.body.firstChild;
    elem.style.height = `${size}px`;
    elem.style.width = `${size}px`;
  }
  /* eslint-enable mozilla/no-cpows-in-tests */

  let sizes = [
    200,
    400,
    300,
  ];

  for (let size of sizes) {
    yield alterContent(browser, setSize, size);
    yield checkSize(size);
  }

  let dims = yield alterContent(browser, setSize, 1400);
  let {body, root} = dims;

  is(dims.window.innerWidth, 800, "Panel window width");
  ok(body.clientWidth <= 800, `Panel body width ${body.clientWidth} is less than 800`);
  is(body.scrollWidth, 1400, "Panel body scroll width");

  is(dims.window.innerHeight, 600, "Panel window height");
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

  /* eslint-disable mozilla/no-cpows-in-tests */
  function setSize(size) {
    content.document.body.style.fontSize = `${size}px`;
  }
  /* eslint-enable mozilla/no-cpows-in-tests */

  let dims = yield alterContent(browser, setSize, 18);

  is(dims.window.innerWidth, 800, "Panel window should be 800px wide");
  is(dims.body.clientWidth, 800, "Panel body should be 800px wide");
  is(dims.body.clientWidth, dims.body.scrollWidth,
     "Panel body should be wide enough to fit its contents");

  ok(dims.window.innerHeight > 36,
     `Panel window height (${dims.window.innerHeight}px) should be taller than two lines of text.`);

  is(dims.body.clientHeight, dims.body.scrollHeight,
    "Panel body should be tall enough to fit its contents");
  is(dims.root.clientHeight, dims.root.scrollHeight,
    "Panel root should be tall enough to fit its contents");

  yield extension.unload();
});
