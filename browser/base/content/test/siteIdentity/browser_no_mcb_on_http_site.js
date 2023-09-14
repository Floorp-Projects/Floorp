/*
 * Description of the Tests for
 *  - Bug 909920 - Mixed content warning should not show on a HTTP site
 *
 * Description of the tests:
 *   Test 1:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file loads an |IMAGE| << over http
 *
 *   Test 2:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file loads a |FONT| over http
 *
 *   Test 3:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file imports (@import) another css file using http
 *     3) The imported css file loads a |FONT| over http
 *
 * Since the top-domain is >> NOT << served using https, the MCB
 * should >> NOT << trigger a warning.
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const PREF_DISPLAY = "security.mixed_content.block_display_content";

const HTTP_TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

var gTestBrowser = null;

function cleanUpAfterTests() {
  gBrowser.removeCurrentTab();
  window.focus();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_ACTIVE, true],
      [PREF_DISPLAY, true],
    ],
  });
  let url = HTTP_TEST_ROOT + "test_no_mcb_on_http_site_img.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  gTestBrowser = tab.linkedBrowser;
});

// ------------- TEST 1 -----------------------------------------

add_task(async function test1() {
  let expected =
    "Verifying MCB does not trigger warning/error for an http page ";
  expected += "with https css that includes http image";

  await SpecialPowers.spawn(
    gTestBrowser,
    [expected],
    async function (condition) {
      await ContentTaskUtils.waitForCondition(
        () => content.document.getElementById("testDiv").innerHTML == condition,
        "Waited too long for status in Test 1!"
      );
    }
  );

  // Explicit OKs needed because the harness requires at least one call to ok.
  ok(true, "test 1 passed");

  // set up test 2
  let url = HTTP_TEST_ROOT + "test_no_mcb_on_http_site_font.html";
  BrowserTestUtils.startLoadingURIString(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

// ------------- TEST 2 -----------------------------------------

add_task(async function test2() {
  let expected =
    "Verifying MCB does not trigger warning/error for an http page ";
  expected += "with https css that includes http font";

  await SpecialPowers.spawn(
    gTestBrowser,
    [expected],
    async function (condition) {
      await ContentTaskUtils.waitForCondition(
        () => content.document.getElementById("testDiv").innerHTML == condition,
        "Waited too long for status in Test 2!"
      );
    }
  );

  ok(true, "test 2 passed");

  // set up test 3
  let url = HTTP_TEST_ROOT + "test_no_mcb_on_http_site_font2.html";
  BrowserTestUtils.startLoadingURIString(gTestBrowser, url);
  await BrowserTestUtils.browserLoaded(gTestBrowser);
});

// ------------- TEST 3 -----------------------------------------

add_task(async function test3() {
  let expected =
    "Verifying MCB does not trigger warning/error for an http page ";
  expected +=
    "with https css that imports another http css which includes http font";

  await SpecialPowers.spawn(
    gTestBrowser,
    [expected],
    async function (condition) {
      await ContentTaskUtils.waitForCondition(
        () => content.document.getElementById("testDiv").innerHTML == condition,
        "Waited too long for status in Test 3!"
      );
    }
  );

  ok(true, "test3 passed");
});

// ------------------------------------------------------

add_task(async function cleanup() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
