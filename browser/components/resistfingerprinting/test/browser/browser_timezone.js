/**
 * Bug 1330890 - A test case for verifying Date() object of javascript will use
 *               UTC timezone after fingerprinting resistance is enabled.
 */

const TEST_DOMAIN = "http://example.net/";
const TEST_PATH = TEST_DOMAIN + "browser/browser/components/resistfingerprinting/test/browser/";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });
});

add_task(async function test_timezone() {
  // Load a page and verify the timezone.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, null,
    async function() {
      let dateObj = new Date();
      let dateString = dateObj.toString();

      ok(dateString.endsWith("(UTC)"), "The date string is in UTC timezone.");
      is(dateObj.getFullYear(), dateObj.getUTCFullYear(),
           "The full year reports in UTC timezone.");
      is(dateObj.getMonth(), dateObj.getUTCMonth(), "The month reports in UTC timezone.");
      is(dateObj.getDate(), dateObj.getUTCDate(), "The month reports in UTC timezone.");
      is(dateObj.getDay(), dateObj.getUTCDay(), "The day reports in UTC timezone.");
      is(dateObj.getHours(), dateObj.getUTCHours(), "The hours reports in UTC timezone.");
      is(dateObj.getTimezoneOffset(), 0, "The difference with UTC timezone is 0.");

    }
  );

  await BrowserTestUtils.removeTab(tab);
});
