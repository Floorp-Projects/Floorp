/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI =
  "http://example.com/browser/dom/tests/browser/browser_frame_elements.html";

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_URI },
    async function (browser) {
      // Confirm its embedder is the browser:
      is(
        browser.browsingContext.embedderElement,
        browser,
        "Embedder element for main window is xul:browser"
      );

      await SpecialPowers.spawn(browser, [], startTests);
    }
  );
});

function startTests() {
  info("Frame tests started");

  info("Checking top window");
  let gWindow = content;
  Assert.equal(gWindow.top, gWindow, "gWindow is top");
  Assert.equal(gWindow.parent, gWindow, "gWindow is parent");

  info("Checking about:blank iframe");
  let iframeBlank = gWindow.document.querySelector("#iframe-blank");
  Assert.ok(iframeBlank, "Iframe exists on page");
  Assert.equal(
    iframeBlank.browsingContext.embedderElement,
    iframeBlank,
    "Embedder element for iframe window is iframe"
  );
  Assert.equal(iframeBlank.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(iframeBlank.contentWindow.parent, gWindow, "gWindow is parent");

  info("Checking iframe with data url src");
  let iframeDataUrl = gWindow.document.querySelector("#iframe-data-url");
  Assert.ok(iframeDataUrl, "Iframe exists on page");
  Assert.equal(
    iframeDataUrl.browsingContext.embedderElement,
    iframeDataUrl,
    "Embedder element for iframe window is iframe"
  );
  Assert.equal(iframeDataUrl.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(
    iframeDataUrl.contentWindow.parent,
    gWindow,
    "gWindow is parent"
  );

  info("Checking object with data url data attribute");
  let objectDataUrl = gWindow.document.querySelector("#object-data-url");
  Assert.ok(objectDataUrl, "Object exists on page");
  Assert.equal(
    objectDataUrl.browsingContext.embedderElement,
    objectDataUrl,
    "Embedder element for object window is the object"
  );
  Assert.equal(objectDataUrl.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(
    objectDataUrl.contentWindow.parent,
    gWindow,
    "gWindow is parent"
  );
}
