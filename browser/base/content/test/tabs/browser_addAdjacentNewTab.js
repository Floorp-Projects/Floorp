/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    // Ensure we can wait for about:newtab to load.
    set: [["browser.newtab.preload", false]],
  });

  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();

  const menuItemOpenANewTab = document.getElementById("context_openANewTab");

  await BrowserTestUtils.switchTab(gBrowser, tab2);

  is(tab1._tPos, 1, "First tab");
  is(tab2._tPos, 2, "Second tab");
  is(tab3._tPos, 3, "Third tab");

  updateTabContextMenu(tab2);
  is(menuItemOpenANewTab.hidden, false, "Open a new Tab is visible");

  const newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  // Open the tab context menu.
  const contextMenu = document.getElementById("tabContextMenu");
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {
    type: "contextmenu",
    button: 2,
  });
  await popupShownPromise;

  EventUtils.synthesizeMouseAtCenter(menuItemOpenANewTab, {});

  let newTab = await newTabPromise;

  is(tab1._tPos, 1, "First tab");
  is(tab2._tPos, 2, "Second tab");
  is(newTab._tPos, 3, "Third tab");
  is(tab3._tPos, 4, "Fourth tab");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(newTab);
});
