/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testPageActionPopupResize() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": "<html><head><meta charset=\"utf-8\"></head></html>",
    },
  });

  yield extension.startup();

  clickBrowserAction(extension, window);

  let {target: panelDocument} = yield BrowserTestUtils.waitForEvent(document, "load", true, (event) => {
    info(`Loaded ${event.target.location}`);
    return event.target.location && event.target.location.href.endsWith("popup.html");
  });

  let panelWindow = panelDocument.defaultView;
  let panelBody = panelDocument.body;

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
    yield BrowserTestUtils.waitForEvent(panelWindow, "resize");
    checkSize(size);
  }

  yield closeBrowserAction(extension);
  yield extension.unload();
});
