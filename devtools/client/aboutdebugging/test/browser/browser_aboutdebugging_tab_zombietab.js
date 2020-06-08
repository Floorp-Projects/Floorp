/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gUniqueCounter = 0;
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

const BROWSER_STATE_TABS = [
  "about:debugging",
  "data:text/html,<title>TEST_TAB_1</title>",
  "data:text/html,<title>TEST_TAB_2</title>",
  "data:text/html,<title>TEST_TAB_3</title>",
];
const BROWSER_STATE = {
  windows: [
    {
      tabs: BROWSER_STATE_TABS.map(url => {
        return {
          entries: [{ url, triggeringPrincipal_base64 }],
          extData: { uniq: Date.now() + "-" + ++gUniqueCounter },
        };
      }),
      selected: 1,
    },
  ],
};

// Check that the inspect action is disabled for lazy/zombie tabs, such as the
// ones created after a session restore.
add_task(async function() {
  // This setup is normally handed by the openAboutDebugging helper, but here we
  // open about:debugging via session restore.
  silenceWorkerUpdates();
  await pushPref("devtools.aboutdebugging.local-tab-debugging", true);

  info("Restore 4 tabs including a selected about:debugging tab");
  const onBrowserSessionRestored = Promise.all([
    TestUtils.topicObserved("sessionstore-browser-state-restored"),
    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored"),
  ]);
  SessionStore.setBrowserState(JSON.stringify(BROWSER_STATE));
  await onBrowserSessionRestored;

  const tab = gBrowser.selectedTab;
  const browser = tab.linkedBrowser;
  const doc = browser.contentDocument;
  const win = browser.contentWindow;
  const store = win.AboutDebugging.store;

  info("Wait until Connect page is displayed");
  await waitUntil(() => doc.querySelector(".qa-connect-page"));

  await selectThisFirefoxPage(doc, store);

  // Check that all inspect butttons are disabled.
  checkInspectButton("TEST_TAB_1", doc, { expectDisabled: true });
  checkInspectButton("TEST_TAB_2", doc, { expectDisabled: true });
  checkInspectButton("TEST_TAB_3", doc, { expectDisabled: true });

  info("Select the TEST_TAB_2 tab top restore it completely");
  const onTabRestored = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "SSTabRestored"
  );
  gBrowser.selectedTab = gBrowser.tabs[2];
  await onTabRestored;

  const onTabsSuccess = waitForDispatch(store, "REQUEST_TABS_SUCCESS");

  info("Select the about:debugging tab again");
  gBrowser.selectedTab = tab;

  info("Wait until the tabs update is finished");
  await onTabsSuccess;

  info("Wait until the inspect button for TEST_TAB_2 is enabled");
  await waitUntil(() => {
    const target = findDebugTargetByText("TEST_TAB_2", doc);
    if (!target) {
      // TEST_TAB_2 target might be missing while the tab target list updates.
      return false;
    }

    const button = target.querySelector(".qa-debug-target-inspect-button");
    return !button.disabled;
  });

  // Check that all inspect butttons are disabled, except for #2.
  checkInspectButton("TEST_TAB_1", doc, { expectDisabled: true });
  checkInspectButton("TEST_TAB_2", doc, { expectDisabled: false });
  checkInspectButton("TEST_TAB_3", doc, { expectDisabled: true });
});

function checkInspectButton(targetText, doc, { expectDisabled }) {
  const inspectButton = getInspectButton(targetText, doc);
  if (expectDisabled) {
    ok(inspectButton.disabled, `Inspect button is disabled for ${targetText}`);
  } else {
    ok(!inspectButton.disabled, `Inspect button is enabled for ${targetText}`);
  }
}

function getInspectButton(targetText, doc) {
  const targetElement = findDebugTargetByText(targetText, doc);
  return targetElement.querySelector(".qa-debug-target-inspect-button");
}
