/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function dismiss_tab_keyboard(closedTab, document) {
  const enter = () => {
    info("Enter");
    EventUtils.synthesizeKey("KEY_Enter");
  };
  const tab = (shiftKey = false) => {
    info(`${shiftKey ? "Shift + Tab" : "Tab"}`);
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
  };
  const closedObjectsChanged = () =>
    TestUtils.topicObserved("sessionstore-closed-objects-changed");
  let firstTabMainContent = closedTab.querySelector(".closed-tab-li-main");
  let dismissButton = closedTab.querySelector(".closed-tab-li-dismiss");
  firstTabMainContent.focus();
  tab();
  Assert.equal(
    document.activeElement,
    dismissButton,
    "Focus should be on the dismiss button for the first item in the recently closed list"
  );
  enter();
  await closedObjectsChanged();
}

/**
 * Tests keyboard navigation of the recently closed tabs component
 */
add_task(async function test_keyboard_navigation() {
  const enter = () => {
    info("Enter");
    EventUtils.synthesizeKey("KEY_Enter");
  };
  const tab = (shiftKey = false) => {
    info(`${shiftKey ? "Shift + Tab" : "Tab"}`);
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
  };
  /**
   * Focus the summary element and asserts that:
   * - The recently closed details should be initially opened
   * - The recently closed details can be opened and closed via the Enter key
   *
   * @param {Document} document The currently used browser's content window document
   * @param {HTMLElement} summary The header section element for recently closed tabs
   */
  const assertPreconditions = (document, summary) => {
    let details = document.getElementById("recently-closed-tabs-container");
    ok(
      details.open,
      "Recently closed details should be initially open on load"
    );
    summary.focus();
    enter();
    ok(!details.open, "Recently closed details should be closed");
    enter();
    ok(details.open, "Recently closed details should be opened");
  };
  await SpecialPowers.clearUserPref(RECENTLY_CLOSED_STATE_PREF);
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );

  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(true);
  await open_then_close(URLs[0]);

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );

    assertPreconditions(document, summary);

    tab();

    Assert.equal(
      list[0].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The first link is focused"
    );

    tab(true);
    Assert.equal(
      summary,
      document.activeElement,
      "The container is focused when using shift+tab in the list"
    );
  });
  // clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }

  clearHistory();

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );
    assertPreconditions(document, summary);

    tab();

    Assert.equal(
      list[0].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The first link is focused"
    );
    tab();
    tab();
    Assert.equal(
      list[1].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The second link is focused"
    );
    tab(true);
    tab(true);
    Assert.equal(
      list[0].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The first link is focused again"
    );

    tab(true);
    Assert.equal(
      summary,
      document.activeElement,
      "The container is focused when using shift+tab in the list"
    );
  });

  // clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }

  clearHistory();

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );
    assertPreconditions(document, summary);

    tab();

    Assert.equal(
      list[0].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The first link is focused"
    );
    tab();
    tab();
    Assert.equal(
      list[1].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The second link is focused"
    );
    tab();
    tab();
    Assert.equal(
      list[2].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The third link is focused"
    );
    tab(true);
    tab(true);
    Assert.equal(
      list[1].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The second link is focused"
    );
    tab(true);
    tab(true);
    Assert.equal(
      list[0].querySelector(".closed-tab-li-main"),
      document.activeElement,
      "The first link is focused"
    );
  });
});

add_task(async function test_dismiss_tab_keyboard() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  Assert.equal(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await open_then_close(URLs[0]);
    await open_then_close(URLs[1]);
    await open_then_close(URLs[2]);

    await EventUtils.synthesizeMouseAtCenter(
      gBrowser.ownerDocument.getElementById("firefox-view-button"),
      { type: "mousedown" },
      window
    );

    const tabsList = document.querySelector("ol.closed-tabs-list");

    await dismiss_tab_keyboard(tabsList.children[0], document);

    Assert.equal(
      tabsList.children[0].dataset.targeturi,
      URLs[1],
      `First recently closed item should be ${URLs[1]}`
    );

    Assert.equal(
      tabsList.children.length,
      2,
      "recently-closed-tabs-list should have two list items"
    );

    await dismiss_tab_keyboard(tabsList.children[0], document);

    Assert.equal(
      tabsList.children[0].dataset.targeturi,
      URLs[0],
      `First recently closed item should be ${URLs[0]}`
    );

    Assert.equal(
      tabsList.children.length,
      1,
      "recently-closed-tabs-list should have one list item"
    );

    await dismiss_tab_keyboard(tabsList.children[0], document);

    Assert.ok(
      document.getElementById("recently-closed-tabs-placeholder"),
      "The empty message is displayed."
    );

    Assert.ok(
      !document.querySelector("ol.closed-tabs-list"),
      "The recently clsoed tabs list is not displayed."
    );
  });
});
