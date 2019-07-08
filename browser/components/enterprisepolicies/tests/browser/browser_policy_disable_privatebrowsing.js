/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      DisablePrivateBrowsing: true,
    },
  });
});

add_task(async function test_menu_shown() {
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
  await BrowserTestUtils.closeWindow(newWin);
});
