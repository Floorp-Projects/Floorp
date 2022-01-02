/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
});

const EN_US_TOPSITES =
  "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/";

// This is used for "sendAttributionRequest"
var gHttpServer = null;
var gRequests = [];

function submitHandler(request, response) {
  gRequests.push(request);
  response.setStatusLine(request.httpVersion, 200, "Ok");
}

// Spy for telemetry sender
let spy;

add_task(async function setup() {
  sandbox = sinon.createSandbox();
  spy = sandbox.spy(
    PartnerLinkAttribution._pingCentre,
    "sendStructuredIngestionPing"
  );

  let topsitesAttribution = Services.prefs.getStringPref(
    "browser.partnerlink.campaign.topsites"
  );
  gHttpServer = new HttpServer();
  gHttpServer.registerPathHandler(`/cid/${topsitesAttribution}`, submitHandler);
  gHttpServer.start(-1);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.sponsoredTopSites", true],
      ["browser.urlbar.suggest.topsites", true],
      ["browser.newtabpage.activity-stream.default.sites", EN_US_TOPSITES],
      [
        "browser.partnerlink.attributionURL",
        `http://localhost:${gHttpServer.identity.primaryPort}/cid/`,
      ],
    ],
  });

  await updateTopSites(
    sites => sites && sites.length == EN_US_TOPSITES.split(",").length
  );

  registerCleanupFunction(async () => {
    sandbox.restore();
    await gHttpServer.stop();
    gHttpServer = null;
  });
});

add_task(async function send_impression_and_click() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let link = {
      label: "test_label",
      url: "http://example.com/",
      sponsored_position: 1,
      sendAttributionRequest: true,
      sponsored_tile_id: 42,
      sponsored_impression_url: "http://impression.test.com/",
      sponsored_click_url: "http://click.test.com/",
    };
    // Pin a sponsored TopSite to set up the test fixture
    NewTabUtils.pinnedLinks.pin(link, 0);

    await updateTopSites(sites => sites && sites[0] && sites[0].isPinned);

    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });

    await UrlbarTestUtils.promiseSearchComplete(window);

    // Select the first result and confirm it.
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    EventUtils.synthesizeKey("KEY_ArrowDown");

    let loadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
      result.url,
      gBrowser.selectedBrowser
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;

    Assert.ok(
      spy.calledTwice,
      "Should send an impression ping and a click ping"
    );

    // Validate the impression ping
    let [payload, endpoint] = spy.firstCall.args;
    Assert.ok(
      endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.TOPSITES_IMPRESSION),
      "Should set the endpoint for TopSites impression"
    );
    Assert.ok(!!payload.context_id, "Should set the context_id");
    Assert.equal(payload.advertiser, "test_label", "Should set the advertiser");
    Assert.equal(
      payload.reporting_url,
      "http://impression.test.com/",
      "Should set the impression reporting URL"
    );
    Assert.equal(payload.tile_id, 42, "Should set the tile_id");
    Assert.equal(payload.position, 1, "Should set the position");

    // Validate the click ping
    [payload, endpoint] = spy.secondCall.args;
    Assert.ok(
      endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.TOPSITES_SELECTION),
      "Should set the endpoint for TopSites click"
    );
    Assert.ok(!!payload.context_id, "Should set the context_id");
    Assert.equal(
      payload.reporting_url,
      "http://click.test.com/",
      "Should set the click reporting URL"
    );
    Assert.equal(payload.tile_id, 42, "Should set the tile_id");
    Assert.equal(payload.position, 1, "Should set the position");

    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });

    NewTabUtils.pinnedLinks.unpin(link);
  });
});

add_task(async function zero_ping() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();

    // Reload the TopSites
    await updateTopSites(
      sites => sites && sites.length == EN_US_TOPSITES.split(",").length
    );

    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });

    await UrlbarTestUtils.promiseSearchComplete(window);

    // Select the first result and confirm it.
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    EventUtils.synthesizeKey("KEY_ArrowDown");

    let loadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
      result.url,
      gBrowser.selectedBrowser
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;

    Assert.ok(
      spy.notCalled,
      "Should not send any ping if there is no sponsored Top Site"
    );

    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
  });
});
