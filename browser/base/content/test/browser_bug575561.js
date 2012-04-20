function test() {
  waitForExplicitFinish();

  // Pinned: Link to the same domain should not open a new tab
  // Tests link to http://example.com/browser/browser/base/content/test/dummy_page.html  
  testLink(0, true, false, function() {
    // Pinned: Link to a different subdomain should open a new tab
    // Tests link to http://test1.example.com/browser/browser/base/content/test/dummy_page.html
    testLink(1, true, true, function() {
      // Pinned: Link to a different domain should open a new tab
      // Tests link to http://example.org/browser/browser/base/content/test/dummy_page.html
      testLink(2, true, true, function() {
        // Not Pinned: Link to a different domain should not open a new tab
        // Tests link to http://example.org/browser/browser/base/content/test/dummy_page.html
        testLink(2, false, false, function() {
          // Pinned: Targetted link should open a new tab
          // Tests link to http://example.org/browser/browser/base/content/test/dummy_page.html with target="foo"
          testLink(3, true, true, function() {
            // Pinned: Link in a subframe should not open a new tab
            // Tests link to http://example.org/browser/browser/base/content/test/dummy_page.html in subframe
            testLink(0, true, false, function() {
              // Pinned: Link to the same domain (with www prefix) should not open a new tab
              // Tests link to http://www.example.com/browser/browser/base/content/test/dummy_page.html      
              testLink(4, true, false, function() {
                // Pinned: Link to a data: URI should not open a new tab
                // Tests link to data:text/html,<!DOCTYPE html><html><body>Another Page</body></html>
                testLink(5, true, false, function() {
                  // Pinned: Link to an about: URI should not open a new tab
                  // Tests link to about:mozilla
                  testLink(6, true, false, finish);
                });
              });
            }, true);
          });
        });
      });
    });
  });
}

function testLink(aLinkIndex, pinTab, expectNewTab, nextTest, testSubFrame) {
  let appTab = gBrowser.addTab("http://example.com/browser/browser/base/content/test/app_bug575561.html", {skipAnimation: true});
  if (pinTab)
    gBrowser.pinTab(appTab);
  gBrowser.selectedTab = appTab;
  appTab.linkedBrowser.addEventListener("load", onLoad, true);

  let loadCount = 0;
  function onLoad() {
    loadCount++;
    if (loadCount < 2)
      return;

    appTab.linkedBrowser.removeEventListener("load", onLoad, true);

    let browser = gBrowser.getBrowserForTab(appTab);
    if (testSubFrame)
      browser = browser.contentDocument.getElementsByTagName("iframe")[0];

    let links = browser.contentDocument.getElementsByTagName("a");

    if (expectNewTab)
      gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);
    else
      browser.addEventListener("load", onPageLoad, true);

    info("Clicking " + links[aLinkIndex].textContent);
    EventUtils.sendMouseEvent({type:"click"}, links[aLinkIndex], browser.contentWindow);
    let linkLocation = links[aLinkIndex].href;

    function onPageLoad() {
      browser.removeEventListener("load", onPageLoad, true);
      is(browser.contentDocument.location.href, linkLocation, "Link should not open in a new tab");
      executeSoon(function(){
        gBrowser.removeTab(appTab);
        nextTest();
      });
    }

    function onTabOpen(event) {
      gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);
      ok(true, "Link should open a new tab");
      executeSoon(function(){
        gBrowser.removeTab(appTab);
        gBrowser.removeCurrentTab();
        nextTest();
      });
    }
  }
}
