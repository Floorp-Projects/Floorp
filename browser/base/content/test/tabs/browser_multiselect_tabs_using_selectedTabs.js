/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true]
    ]
  });

  function testSelectedTabs(tabs) {
    gBrowser.selectedTabs = tabs;
    let {selectedTab, selectedTabs, _multiSelectedTabsSet} = gBrowser;
    is(selectedTab, tabs[0], "The selected tab should be the expected one");
    if (tabs.length == 1) {
      ok(!selectedTab.multiselected, "Selected tab shouldn't be multi-selected because we are not in multi-select context yet");
      ok(!_multiSelectedTabsSet.has(selectedTab), "Selected tab shouldn't be in _multiSelectedTabsSet");
      is(selectedTabs.length, 1, "selectedTabs should contain a single tab");
      is(selectedTabs[0], selectedTab, "selectedTabs should contain the selected tab");
    } else {
      ok(selectedTabs.length, tabs.length, "Check number of selected tabs");
      for (let tab of tabs) {
        ok(tab.multiselected, "Tab should be multi-selected");
        ok(_multiSelectedTabsSet.has(tab), "Tab should be in _multiSelectedTabsSet");
        ok(selectedTabs.includes(tab), "Tab should be in selectedTabs");
      }
    }
  }

  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();

  testSelectedTabs([tab1]);
  testSelectedTabs([tab2]);
  testSelectedTabs([tab2, tab1]);
  testSelectedTabs([tab1, tab2]);
  testSelectedTabs([tab3, tab2]);
  testSelectedTabs([tab3, tab1]);
  testSelectedTabs([tab1, tab2, tab1]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
