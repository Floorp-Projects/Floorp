/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test is for bug 1002724.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1002724
 *
 * When a user enters a host name or IP address in the URL bar, "http" is
 * assumed.  If the host rejects connections on port 80, we try HTTPS as a
 * fall-back and only fail if HTTPS connection fails.
 *
 * This tests that when a user enters "example.com", it attempts to load
 * http://example.com:80 (not rejected), and when trying secureonly.example.com
 * (which rejects connections on port 80), it fails then loads
 * https://secureonly.example.com:443 instead.
 */

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

const bug1002724_tests = [
  {
    original: "example.com",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    expected: "http://example.com",
    explanation: "Should load HTTP version of example.com",
  },
  {
    original: "secureonly.example.com",
    expected: "https://secureonly.example.com",
    explanation:
      "Should reject secureonly.example.com on HTTP but load the HTTPS version",
  },
];

async function test_one(test_obj) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  gURLBar.focus();
  gURLBar.value = test_obj.original;

  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;

  ok(
    tab.linkedBrowser.currentURI.spec.startsWith(test_obj.expected),
    test_obj.explanation
  );

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_bug1002724() {
  await SpecialPowers.pushPrefEnv(
    // Disable HSTS preload just in case.
    {
      set: [
        ["network.stricttransportsecurity.preloadlist", false],
        ["network.dns.native-is-localhost", true],
        ["dom.security.https_first_schemeless", false],
      ],
    }
  );

  for (let test of bug1002724_tests) {
    await test_one(test);
  }

  // Test with HTTPS-First upgrading of schemeless enabled
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first_schemeless", true]],
  });

  bug1002724_tests[0].expected = "https://example.com";
  await test_one(bug1002724_tests[0]);
});
