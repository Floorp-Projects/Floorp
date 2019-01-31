/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests ensures the urlbar reflects the correct value if a page load is
 * stopped immediately after loading.
 */

"use strict";

const goodURL = "http://mochi.test:8888/";
const badURL = "http://mochi.test:8888/whatever.html";

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, goodURL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page");

  await typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page after stop()");
  gBrowser.removeCurrentTab();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  is(gURLBar.textValue, "", "location bar is empty");

  await typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(badURL), "location bar reflects stopped page in an empty tab");
  gBrowser.removeCurrentTab();
});

async function typeAndSubmitAndStop(url) {
  await promiseAutocompleteResultPopup(url, window, true);
  is(gURLBar.textValue, gURLBar.trimValue(url), "location bar reflects loading page");

  let promise =
    BrowserTestUtils.waitForDocLoadAndStopIt(url, gBrowser.selectedBrowser, false);
  gURLBar.handleCommand();
  await promise;
}
