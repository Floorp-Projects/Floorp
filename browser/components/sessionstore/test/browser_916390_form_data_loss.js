/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let tmp;
Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", tmp);
let {TabStateCache} = tmp;

const URL = "http://mochi.test:8888/browser/" +
            "browser/components/sessionstore/test/browser_916390_sample.html";

function test() {
  TestRunner.run();
}

function runTests() {
  // Create a tab with some form fields.
  let tab = gBrowser.selectedTab = gBrowser.addTab(URL);
  let browser = gBrowser.selectedBrowser;
  yield waitForLoad(browser);

  // Modify the text input field's state.
  browser.contentDocument.getElementById("txt").focus();
  EventUtils.synthesizeKey("m", {});
  yield waitForInput();

  // Check that we'll save the form data state correctly.
  let state = JSON.parse(ss.getBrowserState());
  let {formdata} = state.windows[0].tabs[1].entries[0];
  is(formdata.id.txt, "m", "txt's value is correct");

  // Change the number of session history entries to invalidate the cache.
  browser.loadURI(URL + "#");
  TabStateCache.delete(browser);

  // Check that we'll save the form data state correctly.
  let state = JSON.parse(ss.getBrowserState());
  let {formdata} = state.windows[0].tabs[1].entries[1];
  is(formdata.id.txt, "m", "txt's value is correct");

  // Clean up.
  gBrowser.removeTab(tab);
}

function waitForLoad(aElement) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
}

function waitForInput() {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.addMessageListener("SessionStore:input", function onInput() {
    mm.removeMessageListener("SessionStore:input", onInput);
    executeSoon(next);
  });
}

function waitForStorageChange() {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.addMessageListener("SessionStore:MozStorageChanged", function onChanged() {
    mm.removeMessageListener("SessionStore:MozStorageChanged", onChanged);
    executeSoon(next);
  });
}
