/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URLs = [
  "http://mochi.test:8888/browser/",
  "https://www.example.com/",
  "https://example.net/",
  "https://example.org/",
];

const today = new Date();
const yesterday = new Date(
  today.getFullYear(),
  today.getMonth(),
  today.getDate() - 1
);
const dates = [today, yesterday];

let win;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] });
  const pageInfos = URLs.flatMap((url, i) =>
    dates.map(date => ({
      url,
      title: `Example Domain ${i}`,
      visits: [{ date }],
    }))
  );
  await PlacesUtils.history.insertMany(pageInfos);
  win = await BrowserTestUtils.openNewBrowserWindow();
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  await PlacesUtils.history.clear();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_history_cards_created() {
  const { SidebarController } = win;
  await SidebarController.show("viewHistorySidebar");
  const document = SidebarController.browser.contentDocument;
  const component = document.querySelector("sidebar-history");
  await component.updateComplete;
  const { lists } = component;

  Assert.equal(lists.length, dates.length, "There is a card for each day.");
  for (const list of lists) {
    Assert.equal(
      list.tabItems.length,
      URLs.length,
      "Card shows the correct number of visits."
    );
  }

  SidebarController.hide();
});

add_task(async function test_history_search() {
  const { SidebarController } = win;
  await SidebarController.show("viewHistorySidebar");
  const { contentDocument: document, contentWindow } =
    SidebarController.browser;
  const component = document.querySelector("sidebar-history");
  await component.updateComplete;
  const { searchTextbox } = component;

  info("Input a search query.");
  EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, contentWindow);
  EventUtils.sendString("Example Domain 1", contentWindow);
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () =>
      component.lists.length === 1 &&
      component.shadowRoot.querySelector(
        "moz-card[data-l10n-id=sidebar-search-results-header]"
      )
  );
  await TestUtils.waitForCondition(() => {
    const { rowEls } = component.lists[0];
    return rowEls.length === 1 && rowEls[0].mainEl.href === URLs[1];
  }, "There is one matching search result.");

  info("Input a bogus search query.");
  EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, contentWindow);
  EventUtils.sendString("Bogus Query", contentWindow);
  await TestUtils.waitForCondition(() => {
    const tabList = component.lists[0];
    return tabList?.emptyState;
  }, "There are no matching search results.");

  SidebarController.hide();
});
