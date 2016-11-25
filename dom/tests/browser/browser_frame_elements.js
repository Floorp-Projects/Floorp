/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/browser_frame_elements.html";

function getWindowUtils(window) {
  return window.
    QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);
}

add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: TEST_URI }, function* (browser) {
    if (!browser.isRemoteBrowser) {
      // Non-e10s, access contentWindow and confirm its container is the browser:
      let windowUtils = getWindowUtils(browser.contentWindow);
      is (windowUtils.containerElement, browser,
          "Container element for main window is xul:browser");

    }

    yield ContentTask.spawn(browser, null, startTests);
    yield Task.spawn(mozBrowserTests(browser));
  });
});

function startTests() {
  function getWindowUtils(window) {
    return window.
      QueryInterface(Components.interfaces.nsIInterfaceRequestor).
      getInterface(Components.interfaces.nsIDOMWindowUtils);
  }
  info("Frame tests started");

  info("Checking top window");
  let gWindow = content;
  Assert.equal(gWindow.top, gWindow, "gWindow is top");
  Assert.equal(gWindow.parent, gWindow, "gWindow is parent");

  info("Checking about:blank iframe");
  let iframeBlank = gWindow.document.querySelector("#iframe-blank");
  Assert.ok(iframeBlank, "Iframe exists on page");
  let iframeBlankUtils = getWindowUtils(iframeBlank.contentWindow);
  Assert.equal(iframeBlankUtils.containerElement, iframeBlank, "Container element for iframe window is iframe");
  Assert.equal(iframeBlank.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(iframeBlank.contentWindow.parent, gWindow, "gWindow is parent");

  info("Checking iframe with data url src");
  let iframeDataUrl = gWindow.document.querySelector("#iframe-data-url");
  Assert.ok(iframeDataUrl, "Iframe exists on page");
  let iframeDataUrlUtils = getWindowUtils(iframeDataUrl.contentWindow);
  Assert.equal(iframeDataUrlUtils.containerElement, iframeDataUrl, "Container element for iframe window is iframe");
  Assert.equal(iframeDataUrl.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(iframeDataUrl.contentWindow.parent, gWindow, "gWindow is parent");

  info("Checking object with data url data attribute");
  let objectDataUrl = gWindow.document.querySelector("#object-data-url");
  Assert.ok(objectDataUrl, "Object exists on page");
  let objectDataUrlUtils = getWindowUtils(objectDataUrl.contentWindow);
  Assert.equal(objectDataUrlUtils.containerElement, objectDataUrl, "Container element for object window is the object");
  Assert.equal(objectDataUrl.contentWindow.top, gWindow, "gWindow is top");
  Assert.equal(objectDataUrl.contentWindow.parent, gWindow, "gWindow is parent");
}

function* mozBrowserTests(browser) {
  info("Granting special powers for mozbrowser");
  SpecialPowers.addPermission("browser", true, TEST_URI);
  SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);

  yield ContentTask.spawn(browser, null, function() {
    info("Checking mozbrowser iframe");
    let mozBrowserFrame = content.document.createElement("iframe");
    mozBrowserFrame.setAttribute("mozbrowser", "");
    content.document.body.appendChild(mozBrowserFrame);
    Assert.equal(mozBrowserFrame.contentWindow.top, mozBrowserFrame.contentWindow,
        "Mozbrowser top == iframe window");
    Assert.equal(mozBrowserFrame.contentWindow.parent, mozBrowserFrame.contentWindow,
        "Mozbrowser parent == iframe window");
  });

  info("Revoking special powers for mozbrowser");
  SpecialPowers.clearUserPref('dom.mozBrowserFramesEnabled')
  SpecialPowers.removePermission("browser", TEST_URI);
}
