/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });
  const url = "data:text/html,<body>hi";
  await testURL(url, urlEnter);
  await testURL(url, urlClick);
});

function urlEnter(url) {
  gURLBar.value = url;
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
}

function urlClick(url) {
  gURLBar.focus();
  gURLBar.value = "";
  EventUtils.sendString(url);
  EventUtils.synthesizeMouseAtCenter(gURLBar.goButton, {});
}

function promiseNewTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener(
      "TabSwitchDone",
      function() {
        executeSoon(resolve);
      },
      { once: true }
    );
  });
}

function promiseLoaded(browser) {
  return SpecialPowers.spawn(browser, [undefined], async () => {
    if (!["interactive", "complete"].includes(content.document.readyState)) {
      await new Promise(resolve =>
        docShell.chromeEventHandler.addEventListener(
          "DOMContentLoaded",
          resolve,
          {
            once: true,
            capture: true,
          }
        )
      );
    }
  });
}

async function testURL(url, loadFunc, endFunc) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browser = tab.linkedBrowser;

  let pagePrincipal = gBrowser.contentPrincipal;
  // We need to ensure that we set the pageshow event listener before running
  // loadFunc, otherwise there's a chance that the content process will finish
  // loading the page and fire pageshow before the event listener gets set.
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow"
  );
  loadFunc(url);
  await pageShowPromise;

  await SpecialPowers.spawn(
    browser,
    [{ isRemote: gMultiProcessBrowser }],
    async function(arg) {
      Assert.equal(
        Services.focus.focusedElement,
        null,
        "focusedElement not null"
      );
    }
  );

  is(document.activeElement, browser, "content window should be focused");

  ok(
    !gBrowser.contentPrincipal.equals(pagePrincipal),
    "load of " +
      url +
      " by " +
      loadFunc.name +
      " should produce a page with a different principal"
  );

  await BrowserTestUtils.removeTab(tab);
}
