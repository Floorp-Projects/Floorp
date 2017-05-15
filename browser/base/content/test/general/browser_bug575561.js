requestLongerTimeout(2);

const TEST_URL = "http://example.com/browser/browser/base/content/test/general/app_bug575561.html";

add_task(async function() {
  SimpleTest.requestCompleteLog();

  // Pinned: Link to the same domain should not open a new tab
  // Tests link to http://example.com/browser/browser/base/content/test/general/dummy_page.html
  await testLink(0, true, false);
  // Pinned: Link to a different subdomain should open a new tab
  // Tests link to http://test1.example.com/browser/browser/base/content/test/general/dummy_page.html
  await testLink(1, true, true);

  // Pinned: Link to a different domain should open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html
  await testLink(2, true, true);

  // Not Pinned: Link to a different domain should not open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html
  await testLink(2, false, false);

  // Pinned: Targetted link should open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html with target="foo"
  await testLink(3, true, true);

  // Pinned: Link in a subframe should not open a new tab
  // Tests link to http://example.org/browser/browser/base/content/test/general/dummy_page.html in subframe
  await testLink(0, true, false, true);

  // Pinned: Link to the same domain (with www prefix) should not open a new tab
  // Tests link to http://www.example.com/browser/browser/base/content/test/general/dummy_page.html
  await testLink(4, true, false);

  // Pinned: Link to a data: URI should not open a new tab
  // Tests link to data:text/html,<!DOCTYPE html><html><body>Another Page</body></html>
  await testLink(5, true, false);

  // Pinned: Link to an about: URI should not open a new tab
  // Tests link to about:logo
  await testLink(function(doc) {
    let link = doc.createElement("a");
    link.textContent = "Link to Mozilla";
    link.href = "about:logo";
    doc.body.appendChild(link);
    return link;
  }, true, false, false, "about:robots");
});

var waitForPageLoad = async function(browser, linkLocation) {
  await waitForDocLoadComplete();

  is(browser.contentDocument.location.href, linkLocation, "Link should not open in a new tab");
};

var waitForTabOpen = async function() {
  let event = await promiseWaitForEvent(gBrowser.tabContainer, "TabOpen", true);
  ok(true, "Link should open a new tab");

  await waitForDocLoadComplete(event.target.linkedBrowser);
  await Promise.resolve();

  gBrowser.removeCurrentTab();
};

var testLink = async function(aLinkIndexOrFunction, pinTab, expectNewTab, testSubFrame, aURL = TEST_URL) {
  let appTab = BrowserTestUtils.addTab(gBrowser, aURL, {skipAnimation: true});
  if (pinTab)
    gBrowser.pinTab(appTab);
  gBrowser.selectedTab = appTab;

  await waitForDocLoadComplete();

  let browser = appTab.linkedBrowser;
  if (testSubFrame)
    browser = browser.contentDocument.querySelector("iframe");

  let link;
  if (typeof aLinkIndexOrFunction == "function") {
    link = aLinkIndexOrFunction(browser.contentDocument);
  } else {
    link = browser.contentDocument.querySelectorAll("a")[aLinkIndexOrFunction];
  }

  let promise;
  if (expectNewTab)
    promise = waitForTabOpen();
  else
    promise = waitForPageLoad(browser, link.href);

  info("Clicking " + link.textContent);
  link.click();

  await promise;

  gBrowser.removeTab(appTab);
};
