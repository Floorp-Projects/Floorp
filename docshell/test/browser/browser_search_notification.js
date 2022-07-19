/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

add_task(async function() {
  // Our search would be handled by the urlbar normally and not by the docshell,
  // thus we must force going through dns first, so that the urlbar thinks
  // the value may be a url, and asks the docshell to visit it.
  // On NS_ERROR_UNKNOWN_HOST the docshell will fix it up.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.fixup.dns_first_for_single_words", true]],
  });
  const kSearchEngineID = "test_urifixup_search_engine";
  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID,
    search_url: "http://localhost/",
    search_url_get_params: "search={searchTerms}",
  });

  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(
    Services.search.getEngineByName(kSearchEngineID)
  );

  let selectedName = (await Services.search.getDefault()).name;
  Assert.equal(
    selectedName,
    kSearchEngineID,
    "Check fake search engine is selected"
  );

  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.selectedTab = tab;

  gURLBar.value = "firefox";
  gURLBar.handleCommand();

  let [subject, data] = await TestUtils.topicObserved("keyword-search");

  let engine = subject.QueryInterface(Ci.nsISupportsString).data;

  Assert.equal(engine, kSearchEngineID, "Should be the search engine id");
  Assert.equal(data, "firefox", "Notification data is search term.");

  gBrowser.removeTab(tab);
});
