/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Helpers to simulate the use of a single page application.
 */

/* import-globals-from head.js */

/**
 * Used to control the SPA SERP.
 */
class SinglePageAppUtils {
  static async clickAd(tab) {
    info("Clicking ad.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#ad",
      {},
      tab.linkedBrowser
    );
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  static async clickAllTab(tab) {
    info("Click All tab to return to a SERP.");
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#all",
      {},
      tab.linkedBrowser
    );
    await adsPromise;
  }

  static async clickImagesTab(tab) {
    info("Click images tab.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#images",
      {},
      tab.linkedBrowser
    );
    info("Wait a brief amount of time.");
    // There's no obvious way to know we shouldn't expect a SERP impression, so
    // we wait roughly the amount of time it would take for extracting ads to
    // take.
    await promiseWaitForAdLinkCheck();
  }

  static async clickOrganic(tab) {
    info("Clicking organic result.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#organic",
      {},
      tab.linkedBrowser
    );
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  static async clickRedirectAd(tab) {
    info("Clicking redirect ad.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#ad-redirect",
      {},
      tab.linkedBrowser
    );
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  static async clickRedirectAdInNewTab(tab) {
    info("Clicking redirect ad in new tab.");
    let tabPromise = BrowserTestUtils.waitForNewTab(tab.ownerGlobal.gBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#ad-redirect",
      { button: 1 },
      tab.linkedBrowser
    );
    let redirectedTab = await tabPromise;
    return redirectedTab;
  }

  static async clickRedirectAdInNewWindow(tab) {
    let contextMenu = tab.linkedBrowser.ownerGlobal.document.getElementById(
      "contentAreaContextMenu"
    );
    let contextMenuPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    info("Open context menu.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#ad-redirect",
      { type: "contextmenu", button: 2 },
      tab.linkedBrowser
    );
    await contextMenuPromise;

    let newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: "https://example.com/hello_world",
    });
    let openLinkInNewWindow = contextMenu.querySelector("#context-openlink");
    info("Click on Open Link in New Window");
    contextMenu.activateItem(openLinkInNewWindow);
    return await newWindowPromise;
  }

  static async clickSearchbox(tab) {
    info("Clicking searchbox.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#searchbox",
      {},
      tab.linkedBrowser
    );
    await waitForIdle();
  }

  static async clickSearchboxAndType(tab, str = "hello world") {
    await SinglePageAppUtils.clickSearchbox(tab);
    info(`Type ${str} in searchbox.`);
    for (let char of str) {
      await BrowserTestUtils.sendChar(char, tab.linkedBrowser);
    }
    await waitForIdle();
  }

  static async clickSuggestion(tab) {
    info("Clicking the first suggestion.");
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#searchbox-suggestions div",
      {},
      tab.linkedBrowser
    );
    await adsPromise;
  }

  static async clickSuggestionOnImagesTab(tab) {
    info("Clicking the first suggestion on images tab.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#searchbox-suggestions div",
      {},
      tab.linkedBrowser
    );
    await promiseWaitForAdLinkCheck();
  }

  static async createTabAndLoadURL(
    url = new URL(getSERPUrl("searchTelemetrySinglePageApp.html"))
  ) {
    info("Load SERP.");
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url.href);
    await adsPromise;
    return tab;
  }

  static async createTabAndSearch(searchTerm = "test") {
    info("Load SERP.");
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    let url = new URL(getSERPUrl("searchTelemetrySinglePageApp.html"));
    url.searchParams.set("s", searchTerm);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url.href);
    await adsPromise;
    return tab;
  }

  static async createTabsWithDifferentProviders() {
    let url1 = new URL(getSERPUrl("searchTelemetrySinglePageApp.html"));
    let tab1 = await SinglePageAppUtils.createTabAndLoadURL(url1);

    let url2 = new URL(
      getAlternateSERPUrl("searchTelemetrySinglePageApp.html")
    );
    let tab2 = await SinglePageAppUtils.createTabAndLoadURL(url2);

    return [tab1, tab2];
  }

  static async goBack(tab) {
    info("Go back to SERP ads.");
    let promise = TestUtils.topicObserved("reported-page-with-ad-impressions");
    tab.linkedBrowser.goBack();
    await promise;
  }

  static async goBackToPageWithoutAds(tab) {
    info("Go back to SERP without ads.");
    tab.linkedBrowser.goBack();
    await new Promise(resolve => setTimeout(resolve, 200));
  }

  static async goForward(tab) {
    info("Go forward to SERP ads.");
    let promise = TestUtils.topicObserved("reported-page-with-ad-impressions");
    tab.linkedBrowser.goForward();
    await promise;
  }

  static async goForwardToPageWithoutAds(tab) {
    info("Go forward to SERP without ads.");
    tab.linkedBrowser.goForward();
    await new Promise(resolve => setTimeout(resolve, 200));
  }

  static async pushUnrelatedState(tab, { key = "foobar", value = "baz" } = {}) {
    info(`Pushing ${key}=${value} to the list of query parameters in URL.`);
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [key, value],
      async function (contentKey, contentValue) {
        let url = new URL(content.window.location.href);
        url.searchParams.set(contentKey, contentValue);
        content.history.pushState({}, "", url);
      }
    );
  }

  static async visitRelatedSearch(tab) {
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    info("Clicking a related search with an ad.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#related-search",
      {},
      tab.linkedBrowser
    );
    await adsPromise;
  }

  static async visitRelatedSearchWithoutAds(tab) {
    info("Clicking a related search without ads.");
    let adsPromise = TestUtils.topicObserved(
      "reported-page-with-ad-impressions"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#related-search-without-ads",
      {},
      tab.linkedBrowser
    );
    await adsPromise;
  }
}

function getAlternateSERPUrl(page, organic = false) {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + page;
  return `${url}?s=test${organic ? "" : "&abc=ff"}`;
}
