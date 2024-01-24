/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testPageActionPopupResize() {
  let browser;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },
    background: function () {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
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

  await extension.startup();
  await extension.awaitMessage("action-shown");

  clickPageAction(extension, window);

  browser = await awaitExtensionPanel(extension);

  async function checkSize(height, width) {
    let dims = await promiseContentDimensions(browser);
    let { body, root } = dims;

    is(
      dims.window.innerHeight,
      height,
      `Panel window should be ${height}px tall`
    );
    is(
      body.clientHeight,
      body.scrollHeight,
      "Panel body should be tall enough to fit its contents"
    );
    is(
      root.clientHeight,
      root.scrollHeight,
      "Panel root should be tall enough to fit its contents"
    );

    if (width) {
      is(
        body.clientWidth,
        body.scrollWidth,
        "Panel body should be wide enough to fit its contents"
      );

      // Tolerate if it is 1px too wide, as that may happen with the current
      // resizing method.
      Assert.lessOrEqual(
        Math.abs(dims.window.innerWidth - width),
        1,
        `Panel window should be ${width}px wide`
      );
    }
  }

  function setSize(size) {
    let elem = content.document.body.firstElementChild;
    elem.style.height = `${size}px`;
    elem.style.width = `${size}px`;
  }

  function setHeight(height) {
    content.document.body.style.overflow = "hidden";
    let elem = content.document.body.firstElementChild;
    elem.style.height = `${height}px`;
  }

  let sizes = [200, 400, 300];

  for (let size of sizes) {
    await alterContent(browser, setSize, size);
    await checkSize(size, size);
  }

  let dims = await alterContent(browser, setSize, 1400);
  let { body, root } = dims;

  is(dims.window.innerWidth, 800, "Panel window width");
  Assert.lessOrEqual(
    body.clientWidth,
    800,
    `Panel body width ${body.clientWidth} is less than 800`
  );
  is(body.scrollWidth, 1400, "Panel body scroll width");

  is(dims.window.innerHeight, 600, "Panel window height");
  Assert.lessOrEqual(
    root.clientHeight,
    600,
    `Panel root height (${root.clientHeight}px) is less than 600px`
  );
  is(root.scrollHeight, 1400, "Panel root scroll height");

  for (let size of sizes) {
    await alterContent(browser, setHeight, size);
    await checkSize(size, null);
  }

  await extension.unload();
});

add_task(async function testPageActionPopupReflow() {
  let browser;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },
    background: function () {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
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

  await extension.startup();
  await extension.awaitMessage("action-shown");

  clickPageAction(extension, window);

  browser = await awaitExtensionPanel(extension);

  function setSize(size) {
    content.document.body.style.fontSize = `${size}px`;
  }

  let dims = await alterContent(browser, setSize, 18);

  is(dims.window.innerWidth, 800, "Panel window should be 800px wide");
  is(dims.body.clientWidth, 800, "Panel body should be 800px wide");
  is(
    dims.body.clientWidth,
    dims.body.scrollWidth,
    "Panel body should be wide enough to fit its contents"
  );

  Assert.greater(
    dims.window.innerHeight,
    36,
    `Panel window height (${dims.window.innerHeight}px) should be taller than two lines of text.`
  );

  is(
    dims.body.clientHeight,
    dims.body.scrollHeight,
    "Panel body should be tall enough to fit its contents"
  );
  is(
    dims.root.clientHeight,
    dims.root.scrollHeight,
    "Panel root should be tall enough to fit its contents"
  );

  await extension.unload();
});
