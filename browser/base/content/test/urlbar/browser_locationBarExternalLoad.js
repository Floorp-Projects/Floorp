/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function*() {
  const url = "data:text/html,<body>hi";
  yield* testURL(url, urlEnter);
  yield* testURL(url, urlClick);
});

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

function promiseNewTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener("TabSwitchDone", function onSwitch() {
      gBrowser.removeEventListener("TabSwitchDone", onSwitch);
      executeSoon(resolve);
    });
  });
}

function* testURL(url, loadFunc, endFunc) {
  let tabSwitchedPromise = promiseNewTabSwitched();
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;

  let pageshowPromise = BrowserTestUtils.waitForContentEvent(browser, "pageshow");

  yield tabSwitchedPromise;
  yield pageshowPromise;

  let pagePrincipal = gBrowser.contentPrincipal;
  loadFunc(url);

  yield BrowserTestUtils.waitForContentEvent(browser, "pageshow");

  yield ContentTask.spawn(browser, { isRemote: gMultiProcessBrowser },
    function* (arg) {
      const fm = Components.classes["@mozilla.org/focus-manager;1"].
                            getService(Components.interfaces.nsIFocusManager);
      Assert.equal(fm.focusedElement, null, "focusedElement not null");

      if (arg.isRemote) {
        Assert.equal(fm.activeWindow, content, "activeWindow not correct");
      }
  });

  is(document.activeElement, browser, "content window should be focused");

  ok(!gBrowser.contentPrincipal.equals(pagePrincipal),
     "load of " + url + " by " + loadFunc.name + " should produce a page with a different principal");

  gBrowser.removeTab(tab);
}
