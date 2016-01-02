requestLongerTimeout(2);

const TEST_URL = "http://example.com/browser/browser/base/content/test/general/app_bug575561.html";

add_task(function*() {
  SimpleTest.requestCompleteLog();

  // Pinned: Link to the same domain should not open a new tab
  // Tests link to http://example.com/browser/browser/base/content/test/general/dummy_page.html
  yield testLink(0, true, false);
  // Pinned: Link to a different subdomain should open a new tab
  // Tests link to http://test1.example.com/browser/browser/base/content/test/general/dummy_page.html
  yield testLink(1, true, true);

  // Pinned: Link to a different domain should open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html
  yield testLink(2, true, true);

  // Not Pinned: Link to a different domain should not open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html
  yield testLink(2, false, false);

  // Pinned: Targetted link should open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html with target="foo"
  yield testLink(3, true, true);

  // Pinned: Link in a subframe should not open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html in subframe
  yield testLink(0, true, false, true);

  // Pinned: Link to the same domain (with www prefix) should not open a new tab
  // Tests link to http://www.example.com/browser/browser/base/content/test/general/dummy_page.html
  yield testLink(4, true, false);

  // Pinned: Link to a data: URI should not open a new tab
  // Tests link to data:text/html,<!DOCTYPE html><html><body>Another Page</body></html>
  yield testLink(5, true, false);

  // Pinned: Link to an about: URI should not open a new tab
  // Tests link to about:mozilla
  yield testLink(6, true, false);
});

var waitForPageLoad = Task.async(function*(browser, linkLocation) {
  yield waitForDocLoadComplete();

  is(browser.contentDocument.location.href, linkLocation, "Link should not open in a new tab");
});

var waitForTabOpen = Task.async(function*() {
  let event = yield promiseWaitForEvent(gBrowser.tabContainer, "TabOpen", true);
  ok(true, "Link should open a new tab");

  yield waitForDocLoadComplete(event.target.linkedBrowser);
  yield Promise.resolve();

  gBrowser.removeCurrentTab();
});

var testLink = Task.async(function*(aLinkIndex, pinTab, expectNewTab, testSubFrame) {
  let appTab = gBrowser.addTab(TEST_URL, {skipAnimation: true});
  if (pinTab)
    gBrowser.pinTab(appTab);
  gBrowser.selectedTab = appTab;

  yield waitForDocLoadComplete();

  let browser = appTab.linkedBrowser;
  if (testSubFrame)
    browser = browser.contentDocument.querySelector("iframe");

  let link = browser.contentDocument.querySelectorAll("a")[aLinkIndex];

  let promise;
  if (expectNewTab)
    promise = waitForTabOpen();
  else
    promise = waitForPageLoad(browser, link.href);

  info("Clicking " + link.textContent);
  link.click();

  yield promise;

  gBrowser.removeTab(appTab);
});
