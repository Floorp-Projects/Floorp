/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

let dnsService = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
let dnsOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

let { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

let gPort;

function loadHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);
  let body = `
    <!DOCTYPE HTML>
      <html>
        <head>
          <meta charset='utf-8'>
          <title>Ok</title>
        </head>
        <body>
          Ok
        </body>
    </html>`;
  response.bodyOutputStream.write(body, body.length);
}

add_setup(() => {
  dnsOverride.addIPOverride("wubble.com", "127.0.0.1");
  dnsOverride.addIPOverride("wibble.com", "127.0.0.1");

  // Strict native fallback introduces a race condition we can ignore here.
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
  Services.prefs.setBoolPref("network.trr.show_error_page", true);

  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/foo", loadHandler);
  httpServer.start(-1);
  gPort = httpServer.identity.primaryPort;

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.trr.mode");
    Services.prefs.clearUserPref("network.trr.uri");
    Services.prefs.clearUserPref("network.trr.strict_native_fallback");
    Services.prefs.clearUserPref("network.trr.show_error_page");
    dnsOverride.clearHostOverride("wubble.com");
    dnsOverride.clearHostOverride("wibble.com");

    httpServer.stop();
  });
});

async function errorPageLoaded(browser) {
  await BrowserTestUtils.browserLoaded(browser, false, null, true);

  // Wait for the content to layout.
  await SpecialPowers.spawn(browser, [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      let element = content.document.getElementById("learnMoreLink");
      let { width } = element.getBoundingClientRect();
      return width > 0;
    });

    await ContentTaskUtils.waitForCondition(() => {
      let element = content.document.getElementById("retry");
      let { width } = element.getBoundingClientRect();
      return width > 0;
    });
  });
}

async function realPageLoaded(browser) {
  await BrowserTestUtils.browserLoaded(browser);
}

add_task(async function test_reload() {
  let TEST_URL = `http://wubble.com:${gPort}/foo`;

  // Set TRR only mode and use our test server (which won't respond to TRR requests).
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref("network.trr.uri", "https://example.com");

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = errorPageLoaded(browser);
    BrowserTestUtils.loadURI(browser, TEST_URL);
    await loaded;

    let { loadedPage, sourceUrl } = await SpecialPowers.spawn(
      browser,
      [],
      async () => {
        let documentURI = new content.URL(content.document.documentURI);
        return {
          loadedPage: content.document.documentURI.split("?")[0],
          sourceUrl: documentURI.searchParams.get("u"),
        };
      }
    );

    Assert.equal(
      loadedPage,
      "about:doherror",
      "Should have loaded the error page."
    );
    Assert.equal(
      sourceUrl,
      TEST_URL,
      "Should have passed the right source page."
    );

    let sectionVisible = await SpecialPowers.spawn(browser, [], () =>
      ContentTaskUtils.is_visible(
        content.document.getElementById("learn-more-block")
      )
    );
    Assert.ok(!sectionVisible, "Advanced section should be hidden.");

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#learnMoreLink",
      {},
      browser
    );

    await SpecialPowers.spawn(browser, [], () =>
      ContentTaskUtils.waitForCondition(
        () =>
          ContentTaskUtils.is_visible(
            content.document.getElementById("learn-more-block")
          ),
        "Wait for section to show"
      )
    );

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#learnMoreLink",
      {},
      browser
    );

    await SpecialPowers.spawn(browser, [], () =>
      ContentTaskUtils.waitForCondition(
        () =>
          ContentTaskUtils.is_hidden(
            content.document.getElementById("learn-more-block")
          ),
        "Wait for section to hide"
      )
    );

    // Click to retry.
    info("Retrying the load");
    loaded = errorPageLoaded(browser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#retry", {}, browser);
    await loaded;

    ({ loadedPage, sourceUrl } = await SpecialPowers.spawn(
      browser,
      [],
      async () => {
        let documentURI = new content.URL(content.document.documentURI);
        return {
          loadedPage: content.document.documentURI.split("?")[0],
          sourceUrl: documentURI.searchParams.get("u"),
        };
      }
    ));

    Assert.equal(
      loadedPage,
      "about:doherror",
      "Should have loaded the error page."
    );
    Assert.equal(
      sourceUrl,
      TEST_URL,
      "Should have passed the right source page."
    );

    // Manually allow fallback and try again
    Services.prefs.setIntPref("network.trr.mode", 2);
    dnsService.clearCache(true);
    info("Retrying the load");
    loaded = realPageLoaded(browser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#retry", {}, browser);
    await loaded;

    let loadedUrl = await SpecialPowers.spawn(
      browser,
      [],
      () => content.document.documentURI
    );

    Assert.equal(loadedUrl, TEST_URL, "Should have loaded the right page.");
  });
});

add_task(async () => {
  let TEST_URL = `http://wibble.com:${gPort}/foo`;

  // Set TRR only mode and use our test server (which won't respond to TRR requests).
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref("network.trr.uri", "https://example.com");

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = errorPageLoaded(browser);
    BrowserTestUtils.loadURI(browser, TEST_URL);
    await loaded;

    let { loadedPage, sourceUrl } = await SpecialPowers.spawn(
      browser,
      [],
      async () => {
        let documentURI = new content.URL(content.document.documentURI);
        return {
          loadedPage: content.document.documentURI.split("?")[0],
          sourceUrl: documentURI.searchParams.get("u"),
        };
      }
    );

    Assert.equal(
      loadedPage,
      "about:doherror",
      "Should have loaded the error page."
    );
    Assert.equal(
      sourceUrl,
      TEST_URL,
      "Should have passed the right source page."
    );

    // Allow the fallback and reload.
    info("Retrying the load");
    loaded = realPageLoaded(browser);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#allowFallback",
      {},
      browser
    );
    await loaded;

    let loadedUrl = await SpecialPowers.spawn(
      browser,
      [],
      () => content.document.documentURI
    );

    Assert.equal(loadedUrl, TEST_URL, "Should have loaded the right page.");

    Assert.equal(
      Services.prefs.getIntPref("network.trr.mode"),
      2,
      "Should have configured TRR to allow the fallback"
    );
  });
});
