/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests keyboard navigation of the recently closed tabs component
 */
add_task(async function test_keyboard_navigation() {
  const arrowDown = () => {
    info("Arrow down");
    EventUtils.synthesizeKey("KEY_ArrowDown");
  };
  const arrowUp = () => {
    info("Arrow up");
    EventUtils.synthesizeKey("KEY_ArrowUp");
  };
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
    SessionStore.getClosedTabCount(window),
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

  await withFirefoxView({ win: window }, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );

    assertPreconditions(document, summary);
    tab();

    ok(list[0].matches(":focus"), "The first link is focused");
    arrowDown();
    ok(
      list[0].matches(":focus"),
      "The first link is still focused after pressing the down arrow key"
    );
    arrowUp();
    ok(
      list[0].matches(":focus"),
      "The first link is still focused after pressing the up arrow key"
    );

    tab(true);
    ok(
      summary.matches(":focus"),
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

  await withFirefoxView({ win: window }, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );
    assertPreconditions(document, summary);

    tab();

    ok(list[0].matches(":focus"), "The first link is focused");
    arrowDown();
    ok(list[1].matches(":focus"), "The second link is focused");
    arrowUp();
    ok(list[0].matches(":focus"), "The first link is focused again");

    tab(true);
    ok(
      summary.matches(":focus"),
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

  await withFirefoxView({ win: window }, async browser => {
    const { document } = browser.contentWindow;
    const list = document.querySelectorAll(".closed-tab-li");
    let summary = document.getElementById(
      "recently-closed-tabs-header-section"
    );
    assertPreconditions(document, summary);

    tab();

    ok(list[0].matches(":focus"), "The first link is focused");
    arrowDown();
    ok(list[1].matches(":focus"), "The second link is focused");
    arrowDown();
    ok(list[2].matches(":focus"), "The third link is focused");
    arrowDown();
    ok(list[2].matches(":focus"), "The third link is still focused");
    arrowUp();
    ok(list[1].matches(":focus"), "The second link is focused");
    arrowUp();
    ok(list[0].matches(":focus"), "The first link is focused");
    arrowUp();
    ok(list[0].matches(":focus"), "The first link is still focused");

    // Move to an element that is not the first one and ensure
    // focus is moved back to the summary element correctly
    // when using shift+tab
    arrowDown();
    arrowDown();
    arrowDown();
    tab(true);
    ok(
      summary.matches(":focus"),
      "The container is focused when using shift+tab in the list"
    );
  });
});
