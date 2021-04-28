requestLongerTimeout(2);

const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

async function navigateTo(browser, urls, expectedPersist) {
  // Navigate to a bunch of urls
  for (let url of urls) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.loadURI(browser, url);
    await loaded;
  }
  // When we track pageshow event, save the evt.persisted on a doc element,
  // so it can be checked from the test directly.
  let pageShowCheck = evt => {
    evt.target.ownerGlobal.document.documentElement.setAttribute(
      "persisted",
      evt.persisted
    );
    return true;
  };
  is(
    browser.canGoBack,
    true,
    `After navigating to urls=${urls}, we can go back from uri=${browser.currentURI.spec}`
  );
  if (expectedPersist) {
    // If we expect the page to persist, then the uri we are testing is about:blank.
    // Currently we are only testing cases when we go forward to about:blank page,
    // because it gets removed from history if it is sandwiched between two
    // regular history entries. This means we can't test a scenario such as:
    // page X, about:blank, page Y, go back -- about:blank page will be removed, and
    // going back from page Y will take us to page X.

    // Go back from about:blank (it will be the last uri in 'urls')
    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow"
    );
    info(`Navigating back from uri=${browser.currentURI.spec}`);
    browser.goBack();
    await pageShowPromise;
    info(`Got pageshow event`);
    // Now go forward
    let forwardPageShow = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow",
      false,
      pageShowCheck
    );
    info(`Navigating forward from uri=${browser.currentURI.spec}`);
    browser.goForward();
    await forwardPageShow;
    // Check that the page got persisted
    let persisted = await SpecialPowers.spawn(browser, [], async function() {
      return content.document.documentElement.getAttribute("persisted");
    });
    is(
      persisted,
      expectedPersist.toString(),
      `uri ${browser.currentURI.spec} should have persisted`
    );
  } else {
    // Go back
    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow",
      false,
      pageShowCheck
    );
    info(`Navigating back from uri=${browser.currentURI.spec}`);
    browser.goBack();
    await pageShowPromise;
    info(`Got pageshow event`);
    // Check that the page did not get persisted
    let persisted = await SpecialPowers.spawn(browser, [], async function() {
      return content.document.documentElement.getAttribute("persisted");
    });
    is(
      persisted,
      expectedPersist.toString(),
      `uri ${browser.currentURI.spec} shouldn't have persisted`
    );
  }
}

add_task(async function testAboutPagesExemptFromBfcache() {
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({ set: [["fission.bfcacheInParent", true]] });

  // Navigate to a bunch of urls, then go back once, check that the penultimate page did not go into BFbache
  var browser;
  // First page is about:privatebrowsing
  const private_test_cases = [
    ["about:blank"],
    ["about:blank", "about:privatebrowsing", "about:blank"],
  ];
  for (const urls of private_test_cases) {
    info(`Private tab - navigate to ${urls} urls`);
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
      waitForTabURL: "about:privatebrowsing",
    });
    browser = win.gBrowser.selectedTab.linkedBrowser;
    await navigateTo(browser, urls, false);
    await BrowserTestUtils.closeWindow(win);
  }

  // First page is about:blank
  const regular_test_cases = [
    ["about:home"],
    ["about:home", "about:blank"],
    ["about:blank", "about:newtab"],
  ];
  for (const urls of regular_test_cases) {
    info(`Regular tab - navigate to ${urls} urls`);
    let win = await BrowserTestUtils.openNewBrowserWindow();
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser: win.gBrowser,
      opening: "about:newtab",
    });
    browser = tab.linkedBrowser;
    await navigateTo(browser, urls, false);
    BrowserTestUtils.removeTab(tab);
    await BrowserTestUtils.closeWindow(win);
  }
});

// Test that about:blank or pages that have about:* subframes get bfcached.
// TODO bug 1705789: add about:reader tests when we make them bfcache compatible.
add_task(async function testAboutPagesBfcacheAllowed() {
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({ set: [["fission.bfcacheInParent", true]] });

  var browser;
  // First page is about:privatebrowsing
  // about:privatebrowsing -> about:blank, go back, go forward, - about:blank is bfcached
  // about:privatebrowsing -> about:home -> about:blank, go back, go forward, - about:blank is bfcached
  // about:privatebrowsing -> file_about_srcdoc.html,  go back, go forward - file_about_srcdoc.html is bfcached
  const private_test_cases = [
    ["about:blank"],
    ["about:home", "about:blank"],
    [BASE + "file_about_srcdoc.html"],
  ];
  for (const urls of private_test_cases) {
    info(`Private tab - navigate to ${urls} urls`);
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
      waitForTabURL: "about:privatebrowsing",
    });
    browser = win.gBrowser.selectedTab.linkedBrowser;
    await navigateTo(browser, urls, true);
    await BrowserTestUtils.closeWindow(win);
  }

  // First page is about:blank
  // about:blank -> about:home -> about:blank, go back, go forward, - about:blank is bfcached
  // about:blank -> about:home -> file_about_srcdoc.html, go back, go forward - file_about_srcdoc.html is bfcached
  const regular_test_cases = [
    ["about:home", "about:blank"],
    ["about:home", BASE + "file_about_srcdoc.html"],
  ];
  for (const urls of regular_test_cases) {
    info(`Regular tab - navigate to ${urls} urls`);
    let win = await BrowserTestUtils.openNewBrowserWindow();
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser: win.gBrowser,
    });
    browser = tab.linkedBrowser;
    await navigateTo(browser, urls, true);
    BrowserTestUtils.removeTab(tab);
    await BrowserTestUtils.closeWindow(win);
  }
});
