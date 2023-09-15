// Bug 1722489 - HTTPS-Only Mode - Tests evaluation order
// https://bugzilla.mozilla.org/show_bug.cgi?id=1722489
// This test ensures that an http request to an hsts host
// gets upgraded by hsts and not by https-only.
"use strict";

// Set bools to track that tests ended.
let readMessage = false;
let testFinished = false;
// Visit a secure site that sends an HSTS header to set up the rest of the
// test.
add_task(async function see_hsts_header() {
  let setHstsUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "hsts_headers.sjs";
  Services.obs.addObserver(observer, "http-on-examine-response");

  let promiseLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    setHstsUrl
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, setHstsUrl);
  await promiseLoaded;

  await BrowserTestUtils.waitForCondition(() => readMessage);
  // Clean up
  Services.obs.removeObserver(observer, "http-on-examine-response");
});

// Test that HTTPS_Only is not performed if HSTS host is visited.
add_task(async function () {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  Services.console.registerListener(onNewMessage);
  const RESOURCE_LINK =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.com"
    ) + "hsts_headers.sjs";

  // 1. Upgrade page to https://
  let promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    RESOURCE_LINK
  );
  await promiseLoaded;

  await BrowserTestUtils.waitForCondition(() => testFinished);

  // Clean up
  Services.console.unregisterListener(onNewMessage);

  await SpecialPowers.popPrefEnv();
});

// Test that when clicking on #fragment with a different scheme (http vs https)
// DOES cause an actual navigation with HSTS, even though https-only mode is
// enabled.
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", true],
      [
        "dom.security.https_only_mode_break_upgrade_downgrade_endless_loop",
        false,
      ],
    ],
  });

  const TEST_PAGE =
    "http://example.com/browser/dom/security/test/https-only/file_fragment_noscript.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
      waitForLoad: true,
    },
    async function (browser) {
      const UPGRADED_URL = TEST_PAGE.replace("http:", "https:");

      await SpecialPowers.spawn(browser, [UPGRADED_URL], async function (url) {
        is(content.window.location.href, url);

        content.window.addEventListener("scroll", () => {
          ok(false, "scroll event should not trigger");
        });

        let beforeUnload = new Promise(resolve => {
          content.window.addEventListener("beforeunload", resolve, {
            once: true,
          });
        });

        content.window.document.querySelector("#clickMeButton").click();

        // Wait for unload event.
        await beforeUnload;
      });

      await BrowserTestUtils.browserLoaded(browser);

      await SpecialPowers.spawn(browser, [UPGRADED_URL], async function (url) {
        is(content.window.location.href, url + "#foo");
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function () {
  // Reset HSTS header
  readMessage = false;
  let clearHstsUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "hsts_headers.sjs?reset";

  Services.obs.addObserver(observer, "http-on-examine-response");
  // reset hsts header
  let promiseLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    clearHstsUrl
  );
  await BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    clearHstsUrl
  );
  await promiseLoaded;
  await BrowserTestUtils.waitForCondition(() => readMessage);
  // Clean up
  Services.obs.removeObserver(observer, "http-on-examine-response");
});

function observer(subject, topic, state) {
  info("observer called with " + topic);
  if (topic == "http-on-examine-response") {
    onExamineResponse(subject);
  }
}

function onExamineResponse(subject) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  // If message was already read or is not related to "example.com",
  // don't examine it.
  if (!channel.URI.spec.includes("example.com") || readMessage) {
    return;
  }
  info("onExamineResponse with " + channel.URI.spec);
  if (channel.URI.spec.includes("reset")) {
    try {
      let hsts = channel.getResponseHeader("Strict-Transport-Security");
      is(hsts, "max-age=0", "HSTS header is not set");
    } catch (e) {
      ok(false, "HSTS header still set");
    }
    readMessage = true;
    return;
  }
  try {
    let hsts = channel.getResponseHeader("Strict-Transport-Security");
    let csp = channel.getResponseHeader("Content-Security-Policy");
    // Check that HSTS and CSP upgrade headers are set
    is(hsts, "max-age=60", "HSTS header is set");
    is(csp, "upgrade-insecure-requests", "CSP header is set");
  } catch (e) {
    ok(false, "No header set");
  }
  readMessage = true;
}

function onNewMessage(msgObj) {
  const message = msgObj.message;
  // ensure that request is not upgraded HTTPS-Only.
  if (message.includes("Upgrading insecure request")) {
    ok(false, "Top-Level upgrade shouldn't get logged");
    testFinished = true;
  } else if (
    message.includes("Upgrading insecure speculative TCP connection")
  ) {
    // TODO: Check assertion
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1735683
    ok(true, "Top-Level upgrade shouldn't get logged");
    testFinished = true;
  } else if (gBrowser.selectedBrowser.currentURI.scheme === "https") {
    ok(true, "Top-Level upgrade shouldn't get logged");
    testFinished = true;
  }
}
