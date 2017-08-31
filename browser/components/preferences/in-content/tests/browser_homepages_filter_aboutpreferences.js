add_task(async function() {
  is(gBrowser.currentURI.spec, "about:blank", "Test starts with about:blank open");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", null, {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");
  let oldHomepagePref = Services.prefs.getCharPref("browser.startup.homepage");

  let useCurrent = doc.getElementById("useCurrent");
  useCurrent.click();

  is(gBrowser.tabs.length, 3, "Three tabs should be open");
  is(Services.prefs.getCharPref("browser.startup.homepage"), "about:blank|about:home",
     "about:blank and about:home should be the only homepages set");

  Services.prefs.setCharPref("browser.startup.homepage", oldHomepagePref);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
