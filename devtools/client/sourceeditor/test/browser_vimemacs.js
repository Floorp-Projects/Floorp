/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI = "chrome://mochitests/content/browser/devtools/client/sourceeditor/test/codemirror/vimemacs.html";
loadHelperScript("helper_codemirror_runner.js");

function test() {
  requestLongerTimeout(3);
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    runCodeMirrorTest(browser);
  }, true);

  browser.loadURI(URI);
}
