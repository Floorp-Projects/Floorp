/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  const url = "data:text/html,<body>hi";
  await testURL(url, urlEnter);
  await testURL(url, urlClick);
});

function urlEnter(url) {
  gURLBar.value = url;
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}

function urlClick(url) {
  gURLBar.value = url;
  gURLBar.focus();
  EventUtils.synthesizeMouseAtCenter(gURLBar.goButton, {});
}

function promiseNewTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener("TabSwitchDone", function() {
      executeSoon(resolve);
    }, {once: true});
  });
}

async function testURL(url, loadFunc, endFunc) {
  let tabSwitchedPromise = promiseNewTabSwitched();
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.selectedBrowser;

  let pageshowPromise = BrowserTestUtils.waitForContentEvent(browser, "pageshow");

  await tabSwitchedPromise;
  await pageshowPromise;

  let pagePrincipal = gBrowser.contentPrincipal;
  loadFunc(url);

  await BrowserTestUtils.waitForContentEvent(browser, "pageshow");

  await ContentTask.spawn(browser, { isRemote: gMultiProcessBrowser },
    async function(arg) {
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
