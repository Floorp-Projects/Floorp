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

async function testLink(aLinkIndexOrFunction, pinTab, expectNewTab, testSubFrame, aURL = TEST_URL) {
  let appTab = BrowserTestUtils.addTab(gBrowser, aURL, {skipAnimation: true});
  if (pinTab)
    gBrowser.pinTab(appTab);
  gBrowser.selectedTab = appTab;

  let browser = appTab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  let promise;
  if (expectNewTab) {
    promise = BrowserTestUtils.waitForNewTab(gBrowser).then(tab => {
      let loaded = tab.linkedBrowser.documentURI.spec;
      return BrowserTestUtils.removeTab(tab).then(() => loaded);
    });
  } else {
    promise = BrowserTestUtils.browserLoaded(browser, testSubFrame);
  }

  let href;
  if (typeof aLinkIndexOrFunction === "function") {
    ok(!browser.isRemoteBrowser, "don't pass a function for a remote browser");
    let link = aLinkIndexOrFunction(browser.contentDocument);
    info("Clicking " + link.textContent);
    link.click();
    href = link.href;
  } else {
    href = await ContentTask.spawn(browser, [ testSubFrame, aLinkIndexOrFunction ], function([ subFrame, index ]) {
      let doc = subFrame ? content.document.querySelector("iframe").contentDocument : content.document;
      let link = doc.querySelectorAll("a")[index];

      info("Clicking " + link.textContent);
      link.click();
      return link.href;
    });
  }

  info(`Waiting on load of ${href}`);
  let loaded = await promise;
  is(loaded, href, "loaded the right document");
  await BrowserTestUtils.removeTab(appTab);
}
