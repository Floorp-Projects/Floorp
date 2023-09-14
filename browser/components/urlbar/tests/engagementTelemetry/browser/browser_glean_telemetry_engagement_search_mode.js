/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - search_mode

add_setup(async function () {
  await initSearchModeTest();
});

add_task(async function not_search_mode() {
  await doNotSearchModeTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ search_mode: "" }]),
  });
});

add_task(async function search_engine() {
  await doSearchEngineTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ search_mode: "search_engine" }]),
  });
});

add_task(async function bookmarks() {
  await doBookmarksTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ search_mode: "bookmarks" }]),
  });
});

add_task(async function history() {
  await doHistoryTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ search_mode: "history" }]),
  });
});

add_task(async function tabs() {
  await doTabTest({
    trigger: async () => {
      const currentTab = gBrowser.selectedTab;
      EventUtils.synthesizeKey("KEY_Enter");
      await BrowserTestUtils.waitForCondition(
        () => gBrowser.selectedTab !== currentTab
      );
    },
    assert: () => assertEngagementTelemetry([{ search_mode: "tabs" }]),
  });
});

add_task(async function actions() {
  await doActionsTest({
    trigger: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      doClickSubButton(".urlbarView-quickaction-button[data-key=addons]");
      await onLoad;
    },
    assert: () => assertEngagementTelemetry([{ search_mode: "actions" }]),
  });
});
