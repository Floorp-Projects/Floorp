/*
 * Description of the Tests for
 *  - Bug 902156: Persist "disable protection" option for Mixed Content Blocker
 *
 * 1. Navigate to the same domain via document.location
 *    - Load a html page which has mixed content
 *    - Control Center button to disable protection appears - we disable it
 *    - Load a new page from the same origin using document.location
 *    - Control Center button should not appear anymore!
 *
 * 2. Navigate to the same domain via simulateclick for a link on the page
 *    - Load a html page which has mixed content
 *    - Control Center button to disable protection appears - we disable it
 *    - Load a new page from the same origin simulating a click
 *    - Control Center button should not appear anymore!
 *
 * 3. Navigate to a differnet domain and show the content is still blocked
 *    - Load a different html page which has mixed content
 *    - Control Center button to disable protection should appear again because
 *      we navigated away from html page where we disabled the protection.
 *
 * Note, for all tests we set gHttpTestRoot to use 'https'.
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We alternate for even and odd test cases to simulate different hosts.
const HTTPS_TEST_ROOT_1 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test1.example.com"
);
const HTTPS_TEST_ROOT_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test2.example.com"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_ACTIVE, true]] });
});

add_task(async function test1() {
  let url = HTTPS_TEST_ROOT_1 + "file_bug902156_1.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    await assertMixedContentBlockingState(browser, {
      activeLoaded: false,
      activeBlocked: true,
      passiveLoaded: false,
    });

    // Disable Mixed Content Protection for the page (and reload)
    let browserLoaded = BrowserTestUtils.browserLoaded(browser, false, url);
    let { gIdentityHandler } = browser.ownerGlobal;
    gIdentityHandler.disableMixedContentProtection();
    await browserLoaded;

    await SpecialPowers.spawn(browser, [], async function () {
      let expected = "Mixed Content Blocker disabled";
      await ContentTaskUtils.waitForCondition(
        () =>
          content.document.getElementById("mctestdiv").innerHTML == expected,
        "Error: Waited too long for mixed script to run in Test 1"
      );

      let actual = content.document.getElementById("mctestdiv").innerHTML;
      is(
        actual,
        "Mixed Content Blocker disabled",
        "OK: Executed mixed script in Test 1"
      );
    });

    // The Script loaded after we disabled the page, now we are going to reload the
    // page and see if our decision is persistent
    url = HTTPS_TEST_ROOT_1 + "file_bug902156_2.html";
    browserLoaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await browserLoaded;

    // The Control Center button should appear but isMixedContentBlocked should be NOT true,
    // because our decision of disabling the mixed content blocker is persistent.
    await assertMixedContentBlockingState(browser, {
      activeLoaded: true,
      activeBlocked: false,
      passiveLoaded: false,
    });
    await SpecialPowers.spawn(browser, [], function () {
      let actual = content.document.getElementById("mctestdiv").innerHTML;
      is(
        actual,
        "Mixed Content Blocker disabled",
        "OK: Executed mixed script in Test 1"
      );
    });
    gIdentityHandler.enableMixedContentProtection();
  });
});

// ------------------------ Test 2 ------------------------------

add_task(async function test2() {
  let url = HTTPS_TEST_ROOT_2 + "file_bug902156_2.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    await assertMixedContentBlockingState(browser, {
      activeLoaded: false,
      activeBlocked: true,
      passiveLoaded: false,
    });

    // Disable Mixed Content Protection for the page (and reload)
    let browserLoaded = BrowserTestUtils.browserLoaded(browser, false, url);
    let { gIdentityHandler } = browser.ownerGlobal;
    gIdentityHandler.disableMixedContentProtection();
    await browserLoaded;

    await SpecialPowers.spawn(browser, [], async function () {
      let expected = "Mixed Content Blocker disabled";
      await ContentTaskUtils.waitForCondition(
        () =>
          content.document.getElementById("mctestdiv").innerHTML == expected,
        "Error: Waited too long for mixed script to run in Test 2"
      );

      let actual = content.document.getElementById("mctestdiv").innerHTML;
      is(
        actual,
        "Mixed Content Blocker disabled",
        "OK: Executed mixed script in Test 2"
      );
    });

    // The Script loaded after we disabled the page, now we are going to reload the
    // page and see if our decision is persistent
    url = HTTPS_TEST_ROOT_2 + "file_bug902156_1.html";
    browserLoaded = BrowserTestUtils.browserLoaded(browser, false, url);
    // reload the page using the provided link in the html file
    await SpecialPowers.spawn(browser, [], function () {
      let mctestlink = content.document.getElementById("mctestlink");
      mctestlink.click();
    });
    await browserLoaded;

    // The Control Center button should appear but isMixedContentBlocked should be NOT true,
    // because our decision of disabling the mixed content blocker is persistent.
    await assertMixedContentBlockingState(browser, {
      activeLoaded: true,
      activeBlocked: false,
      passiveLoaded: false,
    });

    await SpecialPowers.spawn(browser, [], function () {
      let actual = content.document.getElementById("mctestdiv").innerHTML;
      is(
        actual,
        "Mixed Content Blocker disabled",
        "OK: Executed mixed script in Test 2"
      );
    });
    gIdentityHandler.enableMixedContentProtection();
  });
});

add_task(async function test3() {
  let url = HTTPS_TEST_ROOT_1 + "file_bug902156_3.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    await assertMixedContentBlockingState(browser, {
      activeLoaded: false,
      activeBlocked: true,
      passiveLoaded: false,
    });
  });
});
