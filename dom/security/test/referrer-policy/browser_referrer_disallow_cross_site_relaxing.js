/**
 * Bug 1720294 - Testing disallow relaxing default referrer policy for
 *               cross-site requests.
 */

"use strict";

requestLongerTimeout(5);

const TEST_DOMAIN = "https://example.com/";
const TEST_SAME_SITE_DOMAIN = "https://test1.example.com/";
const TEST_SAME_SITE_DOMAIN_HTTP = "http://test1.example.com/";
const TEST_CROSS_SITE_DOMAIN = "https://test1.example.org/";
const TEST_CROSS_SITE_DOMAIN_HTTP = "http://test1.example.org/";

const TEST_PATH = "browser/dom/security/test/referrer-policy/";

const TEST_PAGE = `${TEST_DOMAIN}${TEST_PATH}referrer_page.sjs`;
const TEST_SAME_SITE_PAGE = `${TEST_SAME_SITE_DOMAIN}${TEST_PATH}referrer_page.sjs`;
const TEST_SAME_SITE_PAGE_HTTP = `${TEST_SAME_SITE_DOMAIN_HTTP}${TEST_PATH}referrer_page.sjs`;
const TEST_CROSS_SITE_PAGE = `${TEST_CROSS_SITE_DOMAIN}${TEST_PATH}referrer_page.sjs`;
const TEST_CROSS_SITE_PAGE_HTTP = `${TEST_CROSS_SITE_DOMAIN_HTTP}${TEST_PATH}referrer_page.sjs`;

const REFERRER_FULL = 0;
const REFERRER_ORIGIN = 1;
const REFERRER_NONE = 2;

function getExpectedReferrer(referrer, type) {
  let res;

  switch (type) {
    case REFERRER_FULL:
      res = referrer;
      break;
    case REFERRER_ORIGIN:
      let url = new URL(referrer);
      res = `${url.origin}/`;
      break;
    case REFERRER_NONE:
      res = "";
      break;
    default:
      ok(false, "unknown type");
  }

  return res;
}

async function verifyResultInPage(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], value => {
    is(content.document.referrer, value, "The document.referrer is correct.");

    let result = content.document.getElementById("result");
    is(result.textContent, value, "The referer header is correct");
  });
}

function getExpectedConsoleMessage(expected, isPrefOn, url) {
  let msg;

  if (isPrefOn) {
    msg =
      "Referrer Policy: Ignoring the less restricted referrer policy “" +
      expected +
      "” for the cross-site request: " +
      url;
  } else {
    msg =
      "Referrer Policy: Less restricted policies, including " +
      "‘no-referrer-when-downgrade’, ‘origin-when-cross-origin’ and " +
      "‘unsafe-url’, will be ignored soon for the cross-site request: " +
      url;
  }

  return msg;
}

function createConsoleMessageVerificationPromise(expected, isPrefOn, url) {
  if (!expected) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    let listener = {
      observe(msg) {
        let message = msg.QueryInterface(Ci.nsIScriptError);

        if (message.category.startsWith("Security")) {
          is(
            message.errorMessage,
            getExpectedConsoleMessage(expected, isPrefOn, url),
            "The console message is correct."
          );
          Services.console.unregisterListener(listener);
          resolve();
        }
      },
    };

    Services.console.registerListener(listener);
  });
}

function verifyNoConsoleMessage() {
  // Verify that there is no referrer policy console message.
  let allMessages = Services.console.getMessageArray();

  for (let msg of allMessages) {
    let message = msg.QueryInterface(Ci.nsIScriptError);
    if (
      message.category.startsWith("Security") &&
      message.errorMessage.startsWith("Referrer Policy:")
    ) {
      ok(false, "There should be no console message for referrer policy.");
    }
  }
}

const TEST_CASES = [
  // Testing that the referrer policy can be overridden with less restricted
  // policy in the same-origin scenario.
  {
    policy: "unsafe-url",
    referrer: TEST_PAGE,
    test_url: TEST_PAGE,
    expect: REFERRER_FULL,
    original: REFERRER_FULL,
  },
  // Testing that the referrer policy can be overridden with less restricted
  // policy in the same-site scenario.
  {
    policy: "unsafe-url",
    referrer: TEST_PAGE,
    test_url: TEST_SAME_SITE_PAGE,
    expect: REFERRER_FULL,
    original: REFERRER_FULL,
  },
  {
    policy: "no-referrer-when-downgrade",
    referrer: TEST_PAGE,
    test_url: TEST_SAME_SITE_PAGE,
    expect: REFERRER_FULL,
    original: REFERRER_FULL,
  },
  {
    policy: "origin-when-cross-origin",
    referrer: TEST_PAGE,
    test_url: TEST_SAME_SITE_PAGE_HTTP,
    expect: REFERRER_ORIGIN,
    original: REFERRER_ORIGIN,
  },
  // Testing that the referrer policy cannot be overridden with less restricted
  // policy in the cross-site scenario.
  {
    policy: "unsafe-url",
    referrer: TEST_PAGE,
    test_url: TEST_CROSS_SITE_PAGE,
    expect: REFERRER_ORIGIN,
    expect_console: "unsafe-url",
    original: REFERRER_FULL,
  },
  {
    policy: "no-referrer-when-downgrade",
    referrer: TEST_PAGE,
    test_url: TEST_CROSS_SITE_PAGE,
    expect: REFERRER_ORIGIN,
    expect_console: "no-referrer-when-downgrade",
    original: REFERRER_FULL,
  },
  {
    policy: "origin-when-cross-origin",
    referrer: TEST_PAGE,
    test_url: TEST_CROSS_SITE_PAGE_HTTP,
    expect: REFERRER_NONE,
    expect_console: "origin-when-cross-origin",
    original: REFERRER_ORIGIN,
  },
  // Testing that the referrer policy can still be overridden with more
  // restricted policy in the cross-site scenario.
  {
    policy: "no-referrer",
    referrer: TEST_PAGE,
    test_url: TEST_CROSS_SITE_PAGE,
    expect: REFERRER_NONE,
    original: REFERRER_NONE,
  },
];

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable mixed content blocking to be able to test downgrade scenario.
      ["security.mixed_content.block_active_content", false],
    ],
  });
});

async function runTestIniFrame(gBrowser, enabled) {
  for (let type of ["meta", "header"]) {
    for (let test of TEST_CASES) {
      info(`Test iframe: ${test.toSource()}`);
      let referrerURL = `${test.referrer}?${type}=${test.policy}`;
      let expected = enabled
        ? getExpectedReferrer(referrerURL, test.expect)
        : getExpectedReferrer(referrerURL, test.original);

      Services.console.reset();

      await BrowserTestUtils.withNewTab(
        { gBrowser, url: referrerURL },
        async browser => {
          let iframeURL = test.test_url + "?show";

          let consolePromise = createConsoleMessageVerificationPromise(
            test.expect_console,
            enabled,
            iframeURL
          );
          // Create an iframe and load the url.
          let bc = await SpecialPowers.spawn(
            browser,
            [iframeURL],
            async url => {
              let iframe = content.document.createElement("iframe");
              iframe.src = url;

              await new content.Promise(resolve => {
                iframe.onload = () => {
                  resolve();
                };

                content.document.body.appendChild(iframe);
              });

              return iframe.browsingContext;
            }
          );

          await verifyResultInPage(bc, expected);
          await consolePromise;
          if (!test.expect_console) {
            verifyNoConsoleMessage();
          }
        }
      );
    }
  }
}

async function runTestForLinkClick(gBrowser, enabled) {
  for (let type of ["meta", "header"]) {
    for (let test of TEST_CASES) {
      info(`Test link click: ${test.toSource()}`);
      let referrerURL = `${test.referrer}?${type}=${test.policy}`;
      let expected = enabled
        ? getExpectedReferrer(referrerURL, test.expect)
        : getExpectedReferrer(referrerURL, test.original);

      Services.console.reset();

      await BrowserTestUtils.withNewTab(
        { gBrowser, url: referrerURL },
        async browser => {
          let linkURL = test.test_url + "?show";

          let consolePromise = createConsoleMessageVerificationPromise(
            test.expect_console,
            enabled,
            linkURL
          );

          // Create the promise to wait for the navigation finishes.
          let loadedPromise = BrowserTestUtils.browserLoaded(
            browser,
            false,
            linkURL
          );

          // Generate the link and click it to navigate.
          await SpecialPowers.spawn(browser, [linkURL], async url => {
            let link = content.document.createElement("a");
            link.textContent = "Link";
            link.setAttribute("href", url);

            content.document.body.appendChild(link);
            link.click();
          });

          await loadedPromise;

          await verifyResultInPage(browser, expected);
          await consolePromise;
          if (!test.expect_console) {
            verifyNoConsoleMessage();
          }
        }
      );
    }
  }
}

add_task(async function test_iframe() {
  for (let enabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["network.http.referer.disallowCrossSiteRelaxingDefault", enabled]],
    });

    await runTestIniFrame(gBrowser, enabled);
  }
});

add_task(async function test_iframe_pbmode() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  for (let enabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "network.http.referer.disallowCrossSiteRelaxingDefault.pbmode",
          enabled,
        ],
      ],
    });

    await runTestIniFrame(win.gBrowser, enabled);
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_link_click() {
  for (let enabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["network.http.referer.disallowCrossSiteRelaxingDefault", enabled]],
    });

    await runTestForLinkClick(gBrowser, enabled);
  }
});

add_task(async function test_link_click_pbmode() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  for (let enabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "network.http.referer.disallowCrossSiteRelaxingDefault.pbmode",
          enabled,
        ],
        // Disable https first mode for private browsing mode to test downgrade
        // cases.
        ["dom.security.https_first_pbm", false],
      ],
    });

    await runTestForLinkClick(win.gBrowser, enabled);
  }

  await BrowserTestUtils.closeWindow(win);
});
