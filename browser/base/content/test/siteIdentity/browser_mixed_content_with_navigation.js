/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the site identity indicator is properly updated when loading from
// the BF cache. This is achieved by loading a page, navigating to another page,
// and then going "back" to the first page, as well as the reverse (loading to
// the other page, navigating to the page we're interested in, going back, and
// then going forward again).

const kBaseURI = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kSecureURI = kBaseURI + "dummy_page.html";

const kTestcases = [
  {
    uri: kBaseURI + "file_mixedPassiveContent.html",
    expectErrorPage: false,
    expectedIdentityMode: "mixedDisplayContent",
  },
  {
    uri: kBaseURI + "file_bug1045809_1.html",
    expectErrorPage: false,
    expectedIdentityMode: "mixedActiveBlocked",
  },
  {
    uri: "https://expired.example.com",
    expectErrorPage: true,
    expectedIdentityMode: "certErrorPage",
  },
];

add_task(async function () {
  for (let testcase of kTestcases) {
    await run_testcase(testcase);
  }
});

async function run_testcase(testcase) {
  await SpecialPowers.pushPrefEnv({
    set: [["security.mixed_content.upgrade_display_content", false]],
  });
  // Test the forward and back case.
  // Start by loading an unrelated URI so that this generalizes well when the
  // testcase would otherwise first navigate to an error page, which doesn't
  // seem to work with withNewTab.
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Navigate to the test URI.
    BrowserTestUtils.startLoadingURIString(browser, testcase.uri);
    if (!testcase.expectErrorPage) {
      await BrowserTestUtils.browserLoaded(browser, false, testcase.uri);
    } else {
      await BrowserTestUtils.waitForErrorPage(browser);
    }
    let identityMode = window.document.getElementById("identity-box").classList;
    ok(
      identityMode.contains(testcase.expectedIdentityMode),
      `identity should be ${testcase.expectedIdentityMode}`
    );

    // Navigate to a URI that should be secure.
    BrowserTestUtils.startLoadingURIString(browser, kSecureURI);
    await BrowserTestUtils.browserLoaded(browser, false, kSecureURI);
    let secureIdentityMode =
      window.document.getElementById("identity-box").className;
    is(secureIdentityMode, "verifiedDomain", "identity should be secure now");

    // Go back to the test page.
    browser.webNavigation.goBack();
    if (!testcase.expectErrorPage) {
      await BrowserTestUtils.browserStopped(browser, testcase.uri);
    } else {
      await BrowserTestUtils.waitForErrorPage(browser);
    }
    let identityModeAgain =
      window.document.getElementById("identity-box").classList;
    ok(
      identityModeAgain.contains(testcase.expectedIdentityMode),
      `identity should again be ${testcase.expectedIdentityMode}`
    );
  });

  // Test the back and forward case.
  // Start on a secure page.
  await BrowserTestUtils.withNewTab(kSecureURI, async browser => {
    let secureIdentityMode =
      window.document.getElementById("identity-box").className;
    is(secureIdentityMode, "verifiedDomain", "identity should start as secure");

    // Navigate to the test URI.
    BrowserTestUtils.startLoadingURIString(browser, testcase.uri);
    if (!testcase.expectErrorPage) {
      await BrowserTestUtils.browserLoaded(browser, false, testcase.uri);
    } else {
      await BrowserTestUtils.waitForErrorPage(browser);
    }
    let identityMode = window.document.getElementById("identity-box").classList;
    ok(
      identityMode.contains(testcase.expectedIdentityMode),
      `identity should be ${testcase.expectedIdentityMode}`
    );

    // Go back to the secure page.
    browser.webNavigation.goBack();
    await BrowserTestUtils.browserStopped(browser, kSecureURI);
    let secureIdentityModeAgain =
      window.document.getElementById("identity-box").className;
    is(
      secureIdentityModeAgain,
      "verifiedDomain",
      "identity should be secure again"
    );

    // Go forward again to the test URI.
    browser.webNavigation.goForward();
    if (!testcase.expectErrorPage) {
      await BrowserTestUtils.browserStopped(browser, testcase.uri);
    } else {
      await BrowserTestUtils.waitForErrorPage(browser);
    }
    let identityModeAgain =
      window.document.getElementById("identity-box").classList;
    ok(
      identityModeAgain.contains(testcase.expectedIdentityMode),
      `identity should again be ${testcase.expectedIdentityMode}`
    );
  });
}
