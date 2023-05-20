/**
 * Bug 1720869 - Testing the referrer policy telemetry.
 */

"use strict";

const TEST_DOMAIN = "https://example.com/";
const TEST_CROSS_SITE_DOMAIN = "https://test1.example.org/";

const TEST_PATH = "browser/dom/security/test/referrer-policy/";

const TEST_PAGE = `${TEST_DOMAIN}${TEST_PATH}referrer_page.sjs`;
const TEST_CROSS_SITE_PAGE = `${TEST_CROSS_SITE_DOMAIN}${TEST_PATH}referrer_page.sjs`;

// This matches to the order in ReferrerPolicy.webidl
const REFERRER_POLICY_INDEX = {
  empty: 0,
  "no-referrer": 1,
  "no-referrer-when-downgrade": 2,
  origin: 3,
  "origin-when-cross-origin": 4,
  "unsafe-url": 5,
  "same-origin": 6,
  "strict-origin": 7,
  "strict-origin-when-cross-origin": 8,
};

const TEST_CASES = [
  {
    policy: "",
    expected: REFERRER_POLICY_INDEX.empty,
  },
  {
    policy: "no-referrer",
    expected: REFERRER_POLICY_INDEX["no-referrer"],
  },
  {
    policy: "no-referrer-when-downgrade",
    expected: REFERRER_POLICY_INDEX["no-referrer-when-downgrade"],
  },
  {
    policy: "origin",
    expected: REFERRER_POLICY_INDEX.origin,
  },
  {
    policy: "origin-when-cross-origin",
    expected: REFERRER_POLICY_INDEX["origin-when-cross-origin"],
  },
  {
    policy: "same-origin",
    expected: REFERRER_POLICY_INDEX["same-origin"],
  },
  {
    policy: "strict-origin",
    expected: REFERRER_POLICY_INDEX["strict-origin"],
  },
  {
    policy: "strict-origin-when-cross-origin",
    expected: REFERRER_POLICY_INDEX["strict-origin-when-cross-origin"],
  },
  {
    policy: "unsafe-url",
    expected: REFERRER_POLICY_INDEX["unsafe-url"],
  },
];

function clearTelemetry() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getHistogramById("REFERRER_POLICY_COUNT").clear();
}

add_setup(async function () {
  // Clear Telemetry probes before testing.
  clearTelemetry();
});

function verifyTelemetry(expected, isSameSite) {
  // The record of cross-site loads is placed in the second half of the
  // telemetry.
  const offset = isSameSite ? 0 : Object.keys(REFERRER_POLICY_INDEX).length;

  let histograms = Services.telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;

  let referrerPolicyCountProbe = histograms.REFERRER_POLICY_COUNT;

  ok(referrerPolicyCountProbe, "The telemetry probe has been recorded.");
  is(
    referrerPolicyCountProbe.values[expected + offset],
    1,
    "The telemetry is added correctly."
  );
}

add_task(async function run_tests() {
  for (let test of TEST_CASES) {
    for (let sameSite of [true, false]) {
      clearTelemetry();
      let referrerURL = `${TEST_PAGE}?header=${test.policy}`;

      await BrowserTestUtils.withNewTab(referrerURL, async browser => {
        let iframeURL = sameSite
          ? TEST_PAGE + "?show"
          : TEST_CROSS_SITE_PAGE + "?show";

        // Create an iframe and load the url.
        await SpecialPowers.spawn(browser, [iframeURL], async url => {
          let iframe = content.document.createElement("iframe");
          iframe.src = url;

          await new content.Promise(resolve => {
            iframe.onload = () => {
              resolve();
            };

            content.document.body.appendChild(iframe);
          });
        });

        verifyTelemetry(test.expected, sameSite);
      });
    }
  }
});
