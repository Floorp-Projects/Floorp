/**
 * Bug 1384330 - A test case for making sure the navigator.mozAddonManager will
 *   be blocked when pref 'privacy.resistFingerprinting.block_mozAddonManager' is true.
 */

const TEST_PATH = "https://example.com/browser/browser/" +
                  "components/resistfingerprinting/test/browser/";

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({"set":
    [["extensions.webapi.testing", true]]
  });

  for (let pref of [false, true]) {
    await SpecialPowers.pushPrefEnv({"set":
      [["privacy.resistFingerprinting.block_mozAddonManager", pref]]
    });

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser, TEST_PATH + "file_dummy.html");

    await ContentTask.spawn(tab.linkedBrowser, pref, function(aPref) {
      if (aPref) {
        is(content.navigator.mozAddonManager, undefined,
          "The navigator.mozAddonManager should not exist when the pref is on.");
      } else {
        ok(content.navigator.mozAddonManager,
          "The navigator.mozAddonManager should exist when the pref is off.");
      }
    });

    await BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
  }
});
