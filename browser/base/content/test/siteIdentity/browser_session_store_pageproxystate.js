/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

let origBrowserState = SessionStore.getBrowserState();

add_setup(async function () {
  registerCleanupFunction(() => {
    SessionStore.setBrowserState(origBrowserState);
  });
});

// Test that when restoring tabs via SessionStore, we directly show the correct
// security state.
add_task(async function test_session_store_security_state() {
  const state = {
    windows: [
      {
        tabs: [
          {
            entries: [
              { url: "https://example.net", triggeringPrincipal_base64 },
            ],
          },
          {
            entries: [
              { url: "https://example.org", triggeringPrincipal_base64 },
            ],
          },
        ],
        selected: 1,
      },
    ],
  };

  // Create a promise that resolves when the tabs have finished restoring.
  let promiseTabsRestored = Promise.all([
    TestUtils.topicObserved("sessionstore-browser-state-restored"),
    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored"),
  ]);

  SessionStore.setBrowserState(JSON.stringify(state));

  await promiseTabsRestored;

  is(gBrowser.selectedTab, gBrowser.tabs[0], "First tab is selected initially");

  info("Switch to second tab which has not been loaded yet.");
  BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[1]);
  is(
    gURLBar.textbox.getAttribute("pageproxystate"),
    "invalid",
    "Page proxy state is invalid after tab switch"
  );

  // Wait for valid pageproxystate. As soon as we have a valid pageproxystate,
  // showing the identity box, it should indicate a secure connection.
  await BrowserTestUtils.waitForMutationCondition(
    gURLBar.textbox,
    {
      attributeFilter: ["pageproxystate"],
    },
    () => gURLBar.textbox.getAttribute("pageproxystate") == "valid"
  );

  // Wait for a tick for security state to apply.
  await new Promise(resolve => setTimeout(resolve, 0));

  is(
    gBrowser.currentURI.spec,
    "https://example.org/",
    "Should have loaded example.org"
  );
  is(
    gIdentityHandler._identityBox.getAttribute("pageproxystate"),
    "valid",
    "identityBox pageproxystate is valid"
  );

  ok(
    gIdentityHandler._isSecureConnection,
    "gIdentityHandler._isSecureConnection is true"
  );
  is(
    gIdentityHandler._identityBox.className,
    "verifiedDomain",
    "identityBox class signals secure connection."
  );
});
