var { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");

add_task(async function testSetHomepageUseCurrent() {
  is(
    gBrowser.currentURI.spec,
    "about:blank",
    "Test starts with about:blank open"
  );
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );
  let oldHomepage = HomePage.get();

  let useCurrent = doc.getElementById("useCurrentBtn");
  useCurrent.click();

  is(gBrowser.tabs.length, 3, "Three tabs should be open");
  await TestUtils.waitForCondition(
    () => HomePage.get() == "about:blank|about:home"
  );
  is(
    HomePage.get(),
    "about:blank|about:home",
    "about:blank and about:home should be the only homepages set"
  );

  HomePage.safeSet(oldHomepage);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
