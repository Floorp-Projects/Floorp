/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_privatebrowsing_disabled() {
  await setupPolicyEngineWithJson({
    policies: {
      PrivateBrowsingModeAvailability: 1,
    },
  });
  is(
    PrivateBrowsingUtils.enabled,
    false,
    "Private browsing should be disabled"
  );
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let privateBrowsingCommand = newWin.document.getElementById(
    "Tools:PrivateBrowsing"
  );
  is(
    privateBrowsingCommand.hidden,
    true,
    "The private browsing command should be hidden"
  );
  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      ok(
        browser.contentDocument.querySelector("menuitem[value='dontremember']")
          .disabled,
        "Don't remember history should be disabled"
      );
    }
  );
  await BrowserTestUtils.closeWindow(newWin);

  await testPageBlockedByPolicy("about:privatebrowsing");
});

add_task(async function test_privatebrowsing_enabled() {
  await setupPolicyEngineWithJson({
    policies: {
      PrivateBrowsingModeAvailability: 2,
    },
  });

  is(PrivateBrowsingUtils.enabled, true, "Private browsing should be true");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      ok(
        browser.contentDocument.getElementById("historyMode").disabled,
        "History dropdown should be disabled"
      );
    }
  );
  await BrowserTestUtils.closeWindow(newWin);
});
