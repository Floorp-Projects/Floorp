/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/browser_frame_elements.html";
let gWindow;

function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab(TEST_URI);
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
    gWindow = null;
  });

  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function test_executeSoon() {
      gWindow = browser.contentWindow;
      startTests();
    });
  }, false);
}

function startTests() {
  info("Frame tests started");

  info("Checking top window");
  let windowUtils = getWindowUtils(gWindow);
  is (windowUtils.containerElement, gBrowser.selectedBrowser, "Container element for main window is xul:browser");
  is (gWindow.top, gWindow, "gWindow is top");
  is (gWindow.parent, gWindow, "gWindow is parent");

  info("Checking about:blank iframe");
  let iframeBlank = gWindow.document.querySelector("#iframe-blank");
  ok (iframeBlank, "Iframe exists on page");
  let iframeBlankUtils = getWindowUtils(iframeBlank.contentWindow);
  is (iframeBlankUtils.containerElement, iframeBlank, "Container element for iframe window is iframe");
  is (iframeBlank.contentWindow.top, gWindow, "gWindow is top");
  is (iframeBlank.contentWindow.parent, gWindow, "gWindow is parent");

  info("Checking iframe with data url src");
  let iframeDataUrl = gWindow.document.querySelector("#iframe-data-url");
  ok (iframeDataUrl, "Iframe exists on page");
  let iframeDataUrlUtils = getWindowUtils(iframeDataUrl.contentWindow);
  is (iframeDataUrlUtils.containerElement, iframeDataUrl, "Container element for iframe window is iframe");
  is (iframeDataUrl.contentWindow.top, gWindow, "gWindow is top");
  is (iframeDataUrl.contentWindow.parent, gWindow, "gWindow is parent");

  info("Checking object with data url data attribute");
  let objectDataUrl = gWindow.document.querySelector("#object-data-url");
  ok (objectDataUrl, "Object exists on page");
  let objectDataUrlUtils = getWindowUtils(objectDataUrl.contentWindow);
  is (objectDataUrlUtils.containerElement, objectDataUrl, "Container element for object window is the object");
  is (objectDataUrl.contentWindow.top, gWindow, "gWindow is top");
  is (objectDataUrl.contentWindow.parent, gWindow, "gWindow is parent");

  info("Granting special powers for mozbrowser");
  SpecialPowers.addPermission("browser", true, gWindow.document);
  SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);

  info("Checking mozbrowser iframe");
  let mozBrowserFrame = gWindow.document.createElement("iframe");
  mozBrowserFrame.setAttribute("mozbrowser", "");
  gWindow.document.body.appendChild(mozBrowserFrame);
  is (mozBrowserFrame.contentWindow.top, mozBrowserFrame.contentWindow, "Mozbrowser top == iframe window");
  is (mozBrowserFrame.contentWindow.parent, mozBrowserFrame.contentWindow, "Mozbrowser parent == iframe window");

  info("Revoking special powers for mozbrowser");
  SpecialPowers.clearUserPref('dom.mozBrowserFramesEnabled')
  SpecialPowers.removePermission("browser", gWindow.document);

  finish();
}

function getWindowUtils(window)
{
  return window.
    QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);
}
