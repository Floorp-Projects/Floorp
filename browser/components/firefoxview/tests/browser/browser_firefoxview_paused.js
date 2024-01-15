/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const tabURL1 = "data:,Tab1";
const tabURL2 = "data:,Tab2";
const tabURL3 = "data:,Tab3";

const TestTabs = {};

function getTopLevelViewElements(document) {
  return {
    recentBrowsingView: document.querySelector(
      "named-deck > view-recentbrowsing"
    ),
    recentlyClosedView: document.querySelector(
      "named-deck > view-recentlyclosed"
    ),
    openTabsView: document.querySelector("named-deck > view-opentabs"),
  };
}

async function getElements(document) {
  let { recentBrowsingView, recentlyClosedView, openTabsView } =
    getTopLevelViewElements(document);
  let recentBrowsingOpenTabsView =
    recentBrowsingView.querySelector("view-opentabs");
  let recentBrowsingOpenTabsList =
    recentBrowsingOpenTabsView?.viewCards[0]?.tabList;
  let recentBrowsingRecentlyClosedTabsView = recentBrowsingView.querySelector(
    "view-recentlyclosed"
  );
  await TestUtils.waitForCondition(
    () => recentBrowsingRecentlyClosedTabsView.fullyUpdated
  );
  let recentBrowsingRecentlyClosedTabsList =
    recentBrowsingRecentlyClosedTabsView?.tabList;
  if (recentlyClosedView.firstUpdateComplete) {
    await TestUtils.waitForCondition(() => recentlyClosedView.fullyUpdated);
  }
  let recentlyClosedList = recentlyClosedView.tabList;
  let openTabsList =
    openTabsView.shadowRoot.querySelector("view-opentabs-card")?.tabList;

  return {
    // recentbrowsing
    recentBrowsingView,
    recentBrowsingOpenTabsView,
    recentBrowsingOpenTabsList,
    recentBrowsingRecentlyClosedTabsView,
    recentBrowsingRecentlyClosedTabsList,

    // recentlyclosed
    recentlyClosedView,
    recentlyClosedList,

    // opentabs
    openTabsView,
    openTabsList,
  };
}

async function nextFrame(global = window) {
  await new Promise(resolve => {
    global.requestAnimationFrame(() => {
      global.requestAnimationFrame(resolve);
    });
  });
}

async function setupOpenAndClosedTabs() {
  TestTabs.tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    tabURL1
  );
  TestTabs.tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    tabURL2
  );
  TestTabs.tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    tabURL3
  );
  // close a tab so we have recently-closed tabs content
  await SessionStoreTestUtils.closeTab(TestTabs.tab3);
}

async function checkFxRenderCalls(browser, elements, selectedView) {
  const sandbox = sinon.createSandbox();
  const topLevelViews = getTopLevelViewElements(browser.contentDocument);

  // sanity-check the selectedView we were given
  ok(
    Object.values(topLevelViews).find(view => view == selectedView),
    `The selected view is in the topLevelViews`
  );

  const elementSpies = new Map();
  const viewSpies = new Map();

  for (let [elemName, elem] of Object.entries(topLevelViews)) {
    let spy;
    if (elem.render.isSinonProxy) {
      spy = elem.render;
    } else {
      info(`Creating spy for render on element: ${elemName}`);
      spy = sandbox.spy(elem, "render");
    }
    viewSpies.set(elem, spy);
  }
  for (let [elemName, elem] of Object.entries(elements)) {
    let spy;
    if (elem.render.isSinonProxy) {
      spy = elem.render;
    } else {
      info(`Creating spy for render on element: ${elemName}`);
      spy = sandbox.spy(elem, "render");
    }
    elementSpies.set(elem, spy);
  }

  info("test switches to tab2");
  await BrowserTestUtils.switchTab(gBrowser, TestTabs.tab2);

  // check all the top-level views are paused
  ok(
    topLevelViews.recentBrowsingView.paused,
    "The recent-browsing view is paused"
  );
  ok(
    topLevelViews.recentlyClosedView.paused,
    "The recently-closed tabs view is paused"
  );
  ok(topLevelViews.openTabsView.paused, "The open tabs view is paused");

  function assertSpiesCalled(spiesMap, expectCalled) {
    let message = expectCalled ? "to be called" : "to not be called";
    for (let [elem, renderSpy] of spiesMap.entries()) {
      is(
        expectCalled,
        renderSpy.called,
        `Expected the render method spy on element ${elem.localName} ${message}`
      );
    }
  }

  await nextFrame();
  info("test removes tab1");
  await BrowserTestUtils.removeTab(TestTabs.tab1);
  await nextFrame();

  assertSpiesCalled(viewSpies, false);
  assertSpiesCalled(elementSpies, false);

  for (let renderSpy of [...viewSpies.values(), ...elementSpies.values()]) {
    renderSpy.resetHistory();
  }

  info("test will re-open fxview");
  await openFirefoxViewTab(window);
  await nextFrame();

  assertSpiesCalled(elementSpies, true);
  ok(
    selectedView.render.called,
    `Render was called on the selected top-level view: ${selectedView.localName}`
  );

  // check all the other views did not render
  viewSpies.delete(selectedView);
  assertSpiesCalled(viewSpies, false);

  sandbox.restore();
}

add_task(async function test_recentbrowsing() {
  await setupOpenAndClosedTabs();

  await withFirefoxView({}, async browser => {
    const document = browser.contentDocument;
    is(document.querySelector("named-deck").selectedViewName, "recentbrowsing");

    const {
      recentBrowsingView,
      recentBrowsingOpenTabsView,
      recentBrowsingOpenTabsList,
      recentBrowsingRecentlyClosedTabsView,
      recentBrowsingRecentlyClosedTabsList,
    } = await getElements(document);

    ok(recentBrowsingView, "Found the recent-browsing view");
    ok(recentBrowsingOpenTabsView, "Found the recent-browsing open tabs view");
    ok(recentBrowsingOpenTabsList, "Found the recent-browsing open tabs list");
    ok(
      recentBrowsingRecentlyClosedTabsView,
      "Found the recent-browsing recently-closed tabs view"
    );
    ok(
      recentBrowsingRecentlyClosedTabsList,
      "Found the recent-browsing recently-closed tabs list"
    );

    // Collapse the Open Tabs card
    let cardContainer = recentBrowsingOpenTabsView.viewCards[0]?.cardEl;
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(
      () => !cardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      recentBrowsingOpenTabsList.updatesPaused,
      "The Open Tabs list is paused after its card is collapsed."
    );
    ok(
      !recentBrowsingOpenTabsList.intervalID,
      "The intervalID for the Open Tabs list is undefined while updates are paused."
    );

    // Expand the Open Tabs card
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(() =>
      cardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      !recentBrowsingOpenTabsList.updatesPaused,
      "The Open Tabs list is unpaused after its card is expanded."
    );
    ok(
      recentBrowsingOpenTabsList.intervalID,
      "The intervalID for the Open Tabs list is defined while updates are unpaused."
    );

    // Collapse the Recently Closed card
    let recentlyClosedCardContainer =
      recentBrowsingRecentlyClosedTabsView.cardEl;
    await EventUtils.synthesizeMouseAtCenter(
      recentlyClosedCardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(
      () => !recentlyClosedCardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      recentBrowsingRecentlyClosedTabsList.updatesPaused,
      "The Recently Closed list is paused after its card is collapsed."
    );
    ok(
      !recentBrowsingRecentlyClosedTabsList.intervalID,
      "The intervalID for the Open Tabs list is undefined while updates are paused."
    );

    // Expand the Recently Closed card
    await EventUtils.synthesizeMouseAtCenter(
      recentlyClosedCardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(() =>
      recentlyClosedCardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      !recentBrowsingRecentlyClosedTabsList.updatesPaused,
      "The Recently Closed list is unpaused after its card is expanded."
    );
    ok(
      recentBrowsingRecentlyClosedTabsList.intervalID,
      "The intervalID for the Recently Closed list is defined while updates are unpaused."
    );

    await checkFxRenderCalls(
      browser,
      {
        recentBrowsingView,
        recentBrowsingOpenTabsView,
        recentBrowsingOpenTabsList,
        recentBrowsingRecentlyClosedTabsView,
        recentBrowsingRecentlyClosedTabsList,
      },
      recentBrowsingView
    );
  });
  await BrowserTestUtils.removeTab(TestTabs.tab2);
});

add_task(async function test_opentabs() {
  await setupOpenAndClosedTabs();

  await withFirefoxView({}, async browser => {
    const document = browser.contentDocument;
    const { openTabsView } = getTopLevelViewElements(document);

    await navigateToCategoryAndWait(document, "opentabs");

    const { openTabsList } = await getElements(document);
    ok(openTabsView, "Found the open tabs view");
    ok(openTabsList, "Found the first open tabs list");
    ok(!openTabsView.paused, "The open tabs view is un-paused");
    is(openTabsView.slot, "selected", "The open tabs view is selected");

    // Collapse the Open Tabs card
    let cardContainer = openTabsView.viewCards[0]?.cardEl;
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(
      () => !cardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      openTabsList.updatesPaused,
      "The Open Tabs list is paused after its card is collapsed."
    );
    ok(
      !openTabsList.intervalID,
      "The intervalID for the Open Tabs list is undefined while updates are paused."
    );

    // Expand the Open Tabs card
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    await TestUtils.waitForCondition(() =>
      cardContainer.detailsEl.hasAttribute("open")
    );

    ok(
      !openTabsList.updatesPaused,
      "The Open Tabs list is unpaused after its card is expanded."
    );
    ok(
      openTabsList.intervalID,
      "The intervalID for the Open Tabs list is defined while updates are unpaused."
    );

    await checkFxRenderCalls(
      browser,
      {
        openTabsView,
        openTabsList,
      },
      openTabsView
    );
  });
  await BrowserTestUtils.removeTab(TestTabs.tab2);
});

add_task(async function test_recentlyclosed() {
  await setupOpenAndClosedTabs();

  await withFirefoxView({}, async browser => {
    const document = browser.contentDocument;
    const { recentlyClosedView } = getTopLevelViewElements(document);
    await navigateToCategoryAndWait(document, "recentlyclosed");

    const { recentlyClosedList } = await getElements(document);
    ok(recentlyClosedView, "Found the recently-closed view");
    ok(recentlyClosedList, "Found the recently-closed list");
    ok(!recentlyClosedView.paused, "The recently-closed view is un-paused");

    await checkFxRenderCalls(
      browser,
      {
        recentlyClosedView,
        recentlyClosedList,
      },
      recentlyClosedView
    );
  });
  await BrowserTestUtils.removeTab(TestTabs.tab2);
});
