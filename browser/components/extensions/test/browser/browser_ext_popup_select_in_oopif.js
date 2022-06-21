/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test is based on browser_ext_popup_select.js.

const iframeSrc = encodeURIComponent(`
<html>
  <style>
  html,body {
    margin: 0;
    padding: 0;
  }
  </style>
  <select>
  <option>Foo</option>
  <option>Bar</option>
  <option>Baz</option>
  </select>
</html>`);

add_task(async function testPopupSelectPopup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: false,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html>
        <html>
          <head><meta charset="utf-8"></head>
          <style>
          html,body {
            margin: 0;
            padding: 0;
          }
          iframe {
            border: none;
          }
          </style>
          <body>
          <iframe src="https://example.com/document-builder.sjs?html=${iframeSrc}">
          </iframe>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  const browserForPopup = await openBrowserActionPanel(
    extension,
    undefined,
    true
  );

  const iframe = await SpecialPowers.spawn(browserForPopup, [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document && content.document.querySelector("iframe");
    });
    const iframeElement = content.document.querySelector("iframe");

    await ContentTaskUtils.waitForCondition(() => {
      return iframeElement.browsingContext;
    });
    return iframeElement.browsingContext;
  });

  const selectRect = await SpecialPowers.spawn(iframe, [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document.querySelector("select");
    });
    const select = content.document.querySelector("select");
    const focusPromise = new Promise(resolve => {
      select.addEventListener("focus", resolve, { once: true });
    });
    select.focus();
    await focusPromise;

    const r = select.getBoundingClientRect();

    return { left: r.left, bottom: r.bottom };
  });

  const popupPromise = BrowserTestUtils.waitForSelectPopupShown(window);

  BrowserTestUtils.synthesizeMouseAtCenter("select", {}, iframe);

  const selectPopup = await popupPromise;

  let popupRect = selectPopup.getOuterScreenRect();
  let popupMarginLeft = parseFloat(getComputedStyle(selectPopup).marginLeft);
  let popupMarginTop = parseFloat(getComputedStyle(selectPopup).marginTop);

  is(
    Math.floor(browserForPopup.screenX + selectRect.left),
    popupRect.left - popupMarginLeft,
    "Select popup has the correct x origin"
  );

  // On Mac select popup window appears on the target select element.
  let expectedY = navigator.platform.includes("Mac")
    ? Math.floor(browserForPopup.screenY)
    : Math.floor(browserForPopup.screenY + selectRect.bottom);
  is(
    expectedY,
    popupRect.top - popupMarginTop,
    "Select popup has the correct y origin"
  );

  const onPopupHidden = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popuphidden"
  );
  selectPopup.hidePopup();
  await onPopupHidden;

  await closeBrowserAction(extension);

  await extension.unload();
});
