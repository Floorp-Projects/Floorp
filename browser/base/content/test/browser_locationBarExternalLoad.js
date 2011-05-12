/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  nextTest();
}

let gTests = [
// XXX bug 656815: javascript: URIs can't currently execute without inheriting
// the page principal
//  {
//    url: "javascript:document.domain;"
//  },
  {
    url: "data:text/html,<script>document.write(document.domain);</script>"
  },
];

function nextTest() {
  let test = gTests.shift();
  if (test)
    testURL(test.url, nextTest);
  else
    finish();
}

function testURL(newURL, func) {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });
  addPageShowListener(function () {
    let pagePrincipal = gBrowser.contentPrincipal;
    gURLBar.value = newURL;
    gURLBar.handleCommand();

    addPageShowListener(function () {
      ok(!gBrowser.contentPrincipal.equals(pagePrincipal), "load of " + newURL + " produced a page with a different principal");
      func();
    });
  });
}

function addPageShowListener(func) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    func();
  });
}
