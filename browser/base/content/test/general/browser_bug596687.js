add_task(async function test() {
  var tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  var gotTabAttrModified = false;
  var gotTabClose = false;

  function onTabClose() {
    gotTabClose = true;
    tab.addEventListener("TabAttrModified", onTabAttrModified);
  }

  function onTabAttrModified() {
    gotTabAttrModified = true;
  }

  tab.addEventListener("TabClose", onTabClose);

  await BrowserTestUtils.removeTab(tab);

  ok(gotTabClose, "should have got the TabClose event");
  ok(!gotTabAttrModified, "shouldn't have got the TabAttrModified event after TabClose");

  tab.removeEventListener("TabClose", onTabClose);
  tab.removeEventListener("TabAttrModified", onTabAttrModified);
});
