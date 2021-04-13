"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const THIRD_PARTY_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.net"
);

const SHIMMABLE_TEST_PAGE = `${TEST_ROOT}shims_test.html`;
const SHIMMABLE_TEST_PAGE_2 = `${TEST_ROOT}shims_test_2.html`;
const SHIMMABLE_TEST_PAGE_3 = `${TEST_ROOT}shims_test_3.html`;
const EMBEDDING_TEST_PAGE = `${THIRD_PARTY_ROOT}iframe_test.html`;

const BLOCKED_TRACKER_URL =
  "//trackertest.org/tests/toolkit/components/url-classifier/tests/mochitest/evil.js";

const DISABLE_SHIM1_PREF = "extensions.webcompat.disabled_shims.MochitestShim";
const DISABLE_SHIM2_PREF = "extensions.webcompat.disabled_shims.MochitestShim2";
const DISABLE_SHIM3_PREF = "extensions.webcompat.disabled_shims.MochitestShim3";
const DISABLE_SHIM4_PREF = "extensions.webcompat.disabled_shims.MochitestShim4";
const GLOBAL_PREF = "extensions.webcompat.enable_shims";
const TRACKING_PREF = "privacy.trackingprotection.enabled";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);

async function testShimRuns(
  testPage,
  frame,
  trackersAllowed = true,
  expectOptIn = true
) {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForLoad: true,
  });

  const TrackingProtection =
    tab.ownerGlobal.gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the tab");
  ok(TrackingProtection.enabled, "TP is enabled");

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [[trackersAllowed, BLOCKED_TRACKER_URL, expectOptIn], frame],
    async (args, _frame) => {
      const window = _frame === undefined ? content : content.frames[_frame];

      await SpecialPowers.spawn(
        window,
        args,
        async (_trackersAllowed, trackerUrl, _expectOptIn) => {
          const shimResult = await content.wrappedJSObject.shimPromise;
          is("shimmed", shimResult, "Shim activated");

          const optInResult = await content.wrappedJSObject.optInPromise;
          is(_expectOptIn, optInResult, "Shim allowed opt in if appropriate");

          const o = content.document.getElementById("shims");
          const cl = o.classList;
          const opts = JSON.parse(o.innerText);
          is(
            undefined,
            opts.branchValue,
            "Shim script did not receive option for other branch"
          );
          is(
            undefined,
            opts.platformValue,
            "Shim script did not receive option for other platform"
          );
          is(
            true,
            opts.simpleOption,
            "Shim script received simple option correctly"
          );
          ok(opts.complexOption, "Shim script received complex option");
          is(
            1,
            opts.complexOption.a,
            "Shim script received complex options correctly #1"
          );
          is(
            "test",
            opts.complexOption.b,
            "Shim script received complex options correctly #2"
          );
          ok(cl.contains("green"), "Shim affected page correctly");
        }
      );
    }
  );

  await BrowserTestUtils.removeTab(tab);
}

async function testShimDoesNotRun(
  trackersAllowed = false,
  testPage = SHIMMABLE_TEST_PAGE
) {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForLoad: true,
  });

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [trackersAllowed, BLOCKED_TRACKER_URL],
    async (_trackersAllowed, trackerUrl) => {
      const shimResult = await content.wrappedJSObject.shimPromise;
      is("did not shim", shimResult, "Shim did not activate");

      ok(
        !content.document.getElementById("shims").classList.contains("green"),
        "Shim script did not run"
      );

      is(
        _trackersAllowed ? "ALLOWED" : "BLOCKED",
        await new Promise(resolve => {
          const s = content.document.createElement("script");
          s.src = trackerUrl;
          s.onload = () => resolve("ALLOWED");
          s.onerror = () => resolve("BLOCKED");
          content.document.head.appendChild(s);
        }),
        "Normally-blocked resources blocked if appropriate"
      );
    }
  );

  await BrowserTestUtils.removeTab(tab);
}
