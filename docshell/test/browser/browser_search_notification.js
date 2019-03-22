/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  const kSearchEngineID = "test_urifixup_search_engine";
  const kSearchEngineURL = "http://localhost/?search={searchTerms}";
  await Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get", kSearchEngineURL);

  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName(kSearchEngineID));

  let selectedName = (await Services.search.getDefault()).name;
  Assert.equal(selectedName, kSearchEngineID, "Check fake search engine is selected");

  registerCleanupFunction(async function() {
    if (oldDefaultEngine) {
      await Services.search.setDefault(oldDefaultEngine);
    }
    let engine = Services.search.getEngineByName(kSearchEngineID);
    if (engine) {
      await Services.search.removeEngine(engine);
    }
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.selectedTab = tab;

  gURLBar.value = "firefox health report";
  gURLBar.handleCommand();

  let [subject, data] = await TestUtils.topicObserved("keyword-search");

  let engine = Services.search.defaultEngine;
  Assert.ok(engine, "Have default search engine.");
  Assert.equal(engine, subject, "Notification subject is engine.");
  Assert.equal(data, "firefox health report", "Notification data is search term.");

  gBrowser.removeTab(tab);
});
