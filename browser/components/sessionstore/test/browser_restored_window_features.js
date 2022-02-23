/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function testFeatures(win, barprops) {
  for (let [name, visible] of Object.entries(barprops)) {
    is(
      win[name].visible,
      visible,
      name + " should be " + (visible ? "visible" : "hidden")
    );
  }
  is(win.toolbar.visible, false, "toolbar should be hidden");
  let toolbar = win.document.getElementById("TabsToolbar");
  is(toolbar.collapsed, true, "tabbar should be collapsed");
  let chromeFlags = win.docShell.treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIAppWindow).chromeFlags;
  is(
    chromeFlags & Ci.nsIWebBrowserChrome.CHROME_WINDOW_RESIZE,
    Ci.nsIWebBrowserChrome.CHROME_WINDOW_RESIZE,
    "window should be resizable"
  );
}

add_task(async function testRestoredWindowFeatures() {
  const DUMMY_PAGE = "browser/base/content/test/tabs/dummy_page.html";
  const TESTS = [
    {
      url: "http://example.com/browser/" + DUMMY_PAGE,
      features: "menubar=0,resizable",
      barprops: { menubar: false },
    },
    {
      url: "data:,", // title should be empty
      features: "location,resizable",
      barprops: { locationbar: true },
    },
  ];
  const TEST_URL_CHROME = "chrome://mochitests/content/browser/" + DUMMY_PAGE;

  for (let test of TESTS) {
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URL_CHROME);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    let newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: test.url,
    });
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [test], t => {
      content.eval(`window.open("${t.url}", "_blank", "${t.features}")`);
    });
    let win = await newWindowPromise;
    let title = win.document.title;

    testFeatures(win, test.barprops);

    await BrowserTestUtils.closeWindow(win);

    newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: test.url,
    });
    SessionStore.undoCloseWindow(0);
    win = await newWindowPromise;

    is(title, win.document.title, "title should be preserved");
    testFeatures(win, test.barprops);

    await BrowserTestUtils.closeWindow(win);
  }
});
