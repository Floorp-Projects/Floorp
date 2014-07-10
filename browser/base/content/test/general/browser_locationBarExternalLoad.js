/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  nextTest();
}

let urls = [
  "data:text/html,<body>hi"
];

function urlEnter(url) {
  gURLBar.value = url;
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}

function urlClick(url) {
  gURLBar.value = url;
  gURLBar.focus();
  let goButton = document.getElementById("urlbar-go-button");
  EventUtils.synthesizeMouseAtCenter(goButton, {});
}

function nextTest() {
  let url = urls.shift();
  if (url) {
    testURL(url, urlEnter, function () {
      testURL(url, urlClick, nextTest);
    });
  }
  else
    finish();
}

function testURL(url, loadFunc, endFunc) {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });
  addPageShowListener(function () {
    let pagePrincipal = gBrowser.contentPrincipal;
    loadFunc(url);

    addPageShowListener(function () {
      let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
      is(fm.focusedElement, null, "should be no focused element");
      is(fm.focusedWindow, gBrowser.contentWindow, "content window should be focused");

      ok(!gBrowser.contentPrincipal.equals(pagePrincipal),
         "load of " + url + " by " + loadFunc.name + " should produce a page with a different principal");
      endFunc();
    });
  });
}

function addPageShowListener(func) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    func();
  });
}
