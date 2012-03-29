const goodURL = "http://mochi.test:8888/";
const badURL = "http://mochi.test:8888/whatever.html";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab(goodURL);
  gBrowser.selectedBrowser.addEventListener("load", onload, true);
}

function onload() {
  gBrowser.selectedBrowser.removeEventListener("load", onload, true);

  is(gURLBar.value, gURLBar.trimValue(goodURL), "location bar reflects loaded page");

  typeAndSubmit(badURL);
  is(gURLBar.value, gURLBar.trimValue(badURL), "location bar reflects loading page");

  gBrowser.contentWindow.stop();
  is(gURLBar.value, gURLBar.trimValue(goodURL), "location bar reflects loaded page after stop()");
  gBrowser.removeCurrentTab();

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  is(gURLBar.value, "", "location bar is empty");

  typeAndSubmit(badURL);
  is(gURLBar.value, gURLBar.trimValue(badURL), "location bar reflects loading page");

  gBrowser.contentWindow.stop();
  is(gURLBar.value, gURLBar.trimValue(badURL), "location bar reflects stopped page in an empty tab");
  gBrowser.removeCurrentTab();

  finish();
}

function typeAndSubmit(value) {
  gBrowser.userTypedValue = value;
  URLBarSetURI();
  gURLBar.handleCommand();
}
