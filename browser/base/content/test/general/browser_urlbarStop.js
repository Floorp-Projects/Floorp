"use strict";

const goodURL = "http://mochi.test:8888/";
const badURL = "http://mochi.test:8888/whatever.html";

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab(goodURL);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page");

  yield typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(goodURL), "location bar reflects loaded page after stop()");
  gBrowser.removeCurrentTab();

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  is(gURLBar.textValue, "", "location bar is empty");

  yield typeAndSubmitAndStop(badURL);
  is(gURLBar.textValue, gURLBar.trimValue(badURL), "location bar reflects stopped page in an empty tab");
  gBrowser.removeCurrentTab();
});

function typeAndSubmitAndStop(url) {
  gBrowser.userTypedValue = url;
  URLBarSetURI();
  is(gURLBar.textValue, gURLBar.trimValue(url), "location bar reflects loading page");

  let promise = waitForDocLoadAndStopIt(url, gBrowser.selectedBrowser, false);
  gURLBar.handleCommand();
  return promise;
}
