/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TopSitesFeed } = ChromeUtils.import(
  "resource://activity-stream/lib/TopSitesFeed.jsm"
);

ChromeUtils.defineESModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  SearchService: "resource://gre/modules/SearchService.sys.mjs",
});

const SHOW_SPONSORED_PREF = "showSponsoredTopSites";
const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";
const CONTILE_CACHE_PREF = "browser.topsites.contile.cachedTiles";
const CONTILE_CACHE_VALID_FOR_PREF = "browser.topsites.contile.cacheValidFor";
const CONTILE_CACHE_LAST_FETCH_PREF = "browser.topsites.contile.lastFetch";
const NIMBUS_VARIABLE_MAX_SPONSORED = "topSitesMaxSponsored";
const NIMBUS_VARIABLE_CONTILE_POSITIONS = "contileTopsitesPositions";
const NIMBUS_VARIABLE_CONTILE_ENABLED = "topSitesContileEnabled";
const NIMBUS_VARIABLE_CONTILE_MAX_NUM_SPONSORED = "topSitesContileMaxSponsored";

let contileTile1 = {
  id: 74357,
  name: "Brand1",
  url: "https://www.brand1.com",
  click_url: "https://clickurl.com",
  image_url: "https://contile-images.jpg",
  image_size: 200,
  impression_url: "https://impression_url.com",
};
let contileTile2 = {
  id: 74925,
  name: "Brand2",
  url: "https://www.brand2.com",
  click_url: "https://click_url.com",
  image_url: "https://contile-images.jpg",
  image_size: 200,
  impression_url: "https://impression_url.com",
};
let contileTile3 = {
  id: 75001,
  name: "Brand3",
  url: "https://www.brand3.com",
  click_url: "https://click_url.com",
  image_url: "https://contile-images.jpg",
  image_size: 200,
  impression_url: "https://impression_url.com",
};
let mozSalesTile = [
  {
    label: "MozSales Title",
    title: "MozSales Title",
    url: "https://mozsale.net",
    sponsored_position: 1,
    partner: "moz-sales",
  },
];

function getTopSitesFeedForTest(sandbox) {
  let feed = new TopSitesFeed();
  const storage = {
    init: sandbox.stub().resolves(),
    get: sandbox.stub().resolves(),
    set: sandbox.stub().resolves(),
  };

  feed._storage = storage;
  feed.store = {
    dispatch: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      Prefs: { values: { topSitesRows: 2 } },
      TopSites: { rows: Array(12).fill("site") },
    },
    dbStorage: { getDbTable: sandbox.stub().returns(storage) },
  };

  return feed;
}

function prepFeed(feed, sandbox) {
  feed.store.state.Prefs.values[SHOW_SPONSORED_PREF] = true;
  let fetchStub = sandbox.stub(feed, "fetch");
  return { feed, fetchStub };
}

function setNimbusVariablesForNumTiles(nimbusPocketStub, numTiles) {
  nimbusPocketStub.withArgs(NIMBUS_VARIABLE_MAX_SPONSORED).returns(numTiles);
  nimbusPocketStub
    .withArgs(NIMBUS_VARIABLE_CONTILE_MAX_NUM_SPONSORED)
    .returns(numTiles);
  // when setting num tiles to > 2 need to set the positions or the > 2 has no effect.
  // can be defaulted to undefined
  let positionsArray = Array.from(
    { length: numTiles },
    (value, index) => index
  );
  nimbusPocketStub
    .withArgs(NIMBUS_VARIABLE_CONTILE_POSITIONS)
    .returns(positionsArray.toString());
}

add_setup(async () => {
  do_get_profile();
  Services.fog.initializeFOG();

  let sandbox = sinon.createSandbox();
  sandbox.stub(SearchService.prototype, "init").resolves();

  const nimbusStub = sandbox.stub(NimbusFeatures.newtab, "getVariable");
  nimbusStub.withArgs(NIMBUS_VARIABLE_CONTILE_ENABLED).returns(true);

  sandbox.spy(Glean.topsites.sponsoredTilesConfigured, "set");
  sandbox.spy(Glean.topsites.sponsoredTilesReceived, "set");

  // Temporarily setting isInAutomation to false.
  // If Cu.isInAutomation is true then the check for Cu.isInAutomation in
  // ContileIntegration._readDefaults passes, bypassing Contile, resulting in
  // not being able use stubbed values.
  if (Cu.isInAutomation) {
    Services.prefs.setBoolPref(
      "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
      false
    );

    if (Cu.isInAutomation) {
      // This condition is unexpected, because it is enforced at:
      // https://searchfox.org/mozilla-central/rev/ea65de7c/js/xpconnect/src/xpcpublic.h#753-759
      throw new Error("Failed to set isInAutomation to false");
    }
  }
  registerCleanupFunction(() => {
    if (!Cu.isInAutomation) {
      Services.prefs.setBoolPref(
        "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
        true
      );

      if (!Cu.isInAutomation) {
        // This condition is unexpected, because it is enforced at:
        // https://searchfox.org/mozilla-central/rev/ea65de7c/js/xpconnect/src/xpcpublic.h#753-759
        throw new Error("Failed to set isInAutomation to true");
      }
    }

    sandbox.restore();
  });
});

add_task(async function test_set_contile_tile_to_oversold() {
  let sandbox = sinon.createSandbox();
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2, contileTile3]);

  let mergedTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      url: "https://www.brand2.com",
      label: "brand2",
      sponsored_position: 2,
      partner: "amp",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToOversold(mergedTiles);
  feed._telemetryUtility.finalizeNewtabPingFields(mergedTiles);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_moz_sale_tile_to_oversold() {
  let sandbox = sinon.createSandbox();
  info(
    "determineFilteredTilesAndSetToOversold should set moz-sale tile to oversold when_contile tiles are displayed"
  );
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2]);
  feed._telemetryUtility.setTiles(mozSalesTile);

  let mergedTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      url: "https://www.brand2.com",
      label: "brand2",
      sponsored_position: 2,
      partner: "amp",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToOversold(mergedTiles);
  feed._telemetryUtility.finalizeNewtabPingFields(mergedTiles);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "mozsales title",
        provider: "moz-sales",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_contile_tile_to_oversold() {
  let sandbox = sinon.createSandbox();
  info(
    "determineFilteredTilesAndSetToOversold should set contile tile to oversold when moz-sale tile is displayed"
  );
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2]);
  feed._telemetryUtility.setTiles(mozSalesTile);
  let mergedTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      label: "MozSales Title",
      title: "MozSales Title",
      url: "https://mozsale.net",
      sponsored_position: 2,
      partner: "moz-sales",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToOversold(mergedTiles);
  feed._telemetryUtility.finalizeNewtabPingFields(mergedTiles);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
      {
        advertiser: "mozsales title",
        provider: "moz-sales",
        display_position: 2,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_contile_tiles_to_dismissed() {
  let sandbox = sinon.createSandbox();
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2, contileTile3]);

  let mergedTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      url: "https://www.brand2.com",
      label: "brand2",
      sponsored_position: 2,
      partner: "amp",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToDismissed(mergedTiles);
  feed._telemetryUtility.finalizeNewtabPingFields(mergedTiles);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_moz_sales_tiles_to_dismissed() {
  let sandbox = sinon.createSandbox();
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2]);
  feed._telemetryUtility.setTiles(mozSalesTile);
  let mergedTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      url: "https://www.brand2.com",
      label: "brand2",
      sponsored_position: 2,
      partner: "amp",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToDismissed(mergedTiles);
  feed._telemetryUtility.finalizeNewtabPingFields(mergedTiles);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "mozsales title",
        provider: "moz-sales",
        display_position: null,
        display_fail_reason: "dismissed",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_position_to_value_gt_3() {
  let sandbox = sinon.createSandbox();
  info("Test setTilePositions uses sponsored_position value, not array index.");
  let feed = getTopSitesFeedForTest(sandbox);

  feed._telemetryUtility.setSponsoredTilesConfigured();
  feed._telemetryUtility.setTiles([contileTile1, contileTile2, contileTile3]);

  let filteredContileTiles = [
    {
      url: "https://www.brand1.com",
      label: "brand1",
      sponsored_position: 1,
      partner: "amp",
    },
    {
      url: "https://www.brand2.com",
      label: "brand2",
      sponsored_position: 2,
      partner: "amp",
    },
    {
      url: "https://www.brand3.com",
      label: "brand3",
      sponsored_position: 6,
      partner: "amp",
    },
  ];

  feed._telemetryUtility.determineFilteredTilesAndSetToOversold(
    filteredContileTiles
  );
  feed._telemetryUtility.finalizeNewtabPingFields(filteredContileTiles);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 6,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_all_tiles_displayed() {
  let sandbox = sinon.createSandbox();
  info("if all provided tiles are displayed, the display_fail_reason is null");
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://www.brand1.com",
            image_url: "images/brand1-com.png",
            click_url: "https://www.brand1-click.com",
            impression_url: "https://www.brand1-impression.com",
            name: "brand1",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
        ],
      }),
  });

  const fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);
  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_one_tile_display_fail_reason_to_oversold() {
  let sandbox = sinon.createSandbox();

  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://www.brand1.com",
            image_url: "images/brand1-com.png",
            click_url: "https://www.brand1-click.com",
            impression_url: "https://www.brand1-impression.com",
            name: "brand1",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brnad3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "brand3",
          },
        ],
      }),
  });

  const fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);
  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_set_one_tile_display_fail_reason_to_dismissed() {
  let sandbox = sinon.createSandbox();
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://foo.com",
            image_url: "images/foo.png",
            click_url: "https://www.foo-click.com",
            impression_url: "https://www.foo-impression.com",
            name: "foo",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brnad2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brand3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "brand3",
          },
        ],
      }),
  });

  Services.prefs.setStringPref(
    TOP_SITES_BLOCKED_SPONSORS_PREF,
    `["foo","bar"]`
  );

  const fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "foo",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
  sandbox.restore();
});

add_task(
  async function test_set_one_tile_to_dismissed_and_one_tile_to_oversold() {
    let sandbox = sinon.createSandbox();
    let { feed, fetchStub } = prepFeed(
      getTopSitesFeedForTest(sandbox),
      sandbox
    );

    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://foo.com",
              image_url: "images/foo.png",
              click_url: "https://www.foo-click.com",
              impression_url: "https://www.foo-impression.com",
              name: "foo",
            },
            {
              url: "https://www.brand2.com",
              image_url: "images/brand2-com.png",
              click_url: "https://www.brand2-click.com",
              impression_url: "https://www.brand2-impression.com",
              name: "brand2",
            },
            {
              url: "https://www.brand3.com",
              image_url: "images/brand3-com.png",
              click_url: "https://www.brand3-click.com",
              impression_url: "https://www.brand3-impression.com",
              name: "brand3",
            },
            {
              url: "https://www.brand4.com",
              image_url: "images/brand4-com.png",
              click_url: "https://www.brand4-click.com",
              impression_url: "https://www.brand4-impression.com",
              name: "brand4",
            },
          ],
        }),
    });

    Services.prefs.setStringPref(
      TOP_SITES_BLOCKED_SPONSORS_PREF,
      `["foo","bar"]`
    );

    const fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 2);

    await feed._readDefaults();
    await feed.getLinksWithDefaults(false);

    Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

    let expectedResult = {
      sponsoredTilesReceived: [
        {
          advertiser: "foo",
          provider: "amp",
          display_position: null,
          display_fail_reason: "dismissed",
        },
        {
          advertiser: "brand2",
          provider: "amp",
          display_position: 1,
          display_fail_reason: null,
        },
        {
          advertiser: "brand3",
          provider: "amp",
          display_position: 2,
          display_fail_reason: null,
        },
        {
          advertiser: "brand4",
          provider: "amp",
          display_position: null,
          display_fail_reason: "oversold",
        },
      ],
    };
    Assert.equal(
      Glean.topsites.sponsoredTilesReceived.testGetValue(),
      JSON.stringify(expectedResult)
    );
    Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
    sandbox.restore();
  }
);

add_task(
  async function test_set_one_cached_tile_display_fail_reason_to_dismissed() {
    let sandbox = sinon.createSandbox();
    info("confirm the telemetry is valid when using cached tiles.");

    let { feed, fetchStub } = prepFeed(
      getTopSitesFeedForTest(sandbox),
      sandbox
    );

    const tiles = [
      {
        url: "https://www.brand1.com",
        image_url: "images/brand1-com.png",
        click_url: "https://www.brand1-click.com",
        impression_url: "https://www.brand1-impression.com",
        name: "brand1",
      },
      {
        url: "https://foo.com",
        image_url: "images/foo-com.png",
        click_url: "https://www.foo-click.com",
        impression_url: "https://www.foo-impression.com",
        name: "foo",
      },
    ];

    Services.prefs.setStringPref(CONTILE_CACHE_PREF, JSON.stringify(tiles));
    Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_PREF, 60 * 15);
    Services.prefs.setIntPref(
      CONTILE_CACHE_LAST_FETCH_PREF,
      Math.round(Date.now() / 1000)
    );
    Services.prefs.setStringPref(
      TOP_SITES_BLOCKED_SPONSORS_PREF,
      `["foo","bar"]`
    );

    fetchStub.resolves({
      status: 304,
    });

    const fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 1);

    await feed._readDefaults();
    await feed.getLinksWithDefaults(false);

    Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

    let expectedResult = {
      sponsoredTilesReceived: [
        {
          advertiser: "brand1",
          provider: "amp",
          display_position: 1,
          display_fail_reason: null,
        },
        {
          advertiser: "foo",
          provider: "amp",
          display_position: null,
          display_fail_reason: "dismissed",
        },
      ],
    };
    Assert.equal(
      Glean.topsites.sponsoredTilesReceived.testGetValue(),
      JSON.stringify(expectedResult)
    );
    Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
    sandbox.restore();
  }
);

add_task(async function test_update_tile_count() {
  let sandbox = sinon.createSandbox();
  info(
    "the tile count should update when topSitesMaxSponsored is updated by Nimbus"
  );
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://www.brand1.com",
            image_url: "images/brand1-com.png",
            click_url: "https://www.brand1-click.com",
            impression_url: "https://www.brand1-impression.com",
            name: "brand1",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brand3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "brand3",
          },
        ],
      }),
  });

  // 1.  Initially the Nimbus pref is set to 2 tiles
  let fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);
  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  // 2.  Set the Numbus pref to 3, confirm previous count still used.
  const nimbusPocketStub = sandbox.stub(
    NimbusFeatures.pocketNewtab,
    "getVariable"
  );
  setNimbusVariablesForNumTiles(nimbusPocketStub, 3);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  // 3.  Confirm the new count is applied when data pulled from Contile., 3 tiles displayed
  fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 3);
  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 3);

  expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 3,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  sandbox.restore();
});

add_task(async function test_update_tile_count_sourced_from_cache() {
  let sandbox = sinon.createSandbox();

  info(
    "the tile count should update from cache when topSitesMaxSponsored is updated by Nimbus"
  );
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  const tiles = [
    {
      url: "https://www.brand1.com",
      image_url: "images/brand1-com.png",
      click_url: "https://www.brand1-click.com",
      impression_url: "https://www.brand1-impression.com",
      name: "brand1",
    },
    {
      url: "https://www.brand2.com",
      image_url: "images/brand2-com.png",
      click_url: "https://www.brand2-click.com",
      impression_url: "https://www.brand2-impression.com",
      name: "brand2",
    },
    {
      url: "https://www.brand3.com",
      image_url: "images/brand3-com.png",
      click_url: "https://www.brand3-click.com",
      impression_url: "https://www.brand3-impression.com",
      name: "brand3",
    },
  ];

  Services.prefs.setStringPref(CONTILE_CACHE_PREF, JSON.stringify(tiles));
  Services.prefs.setIntPref(CONTILE_CACHE_VALID_FOR_PREF, 60 * 15);
  Services.prefs.setIntPref(
    CONTILE_CACHE_LAST_FETCH_PREF,
    Math.round(Date.now() / 1000)
  );

  fetchStub.resolves({
    status: 304,
  });

  // 1.  Initially the Nimbus pref is set to 2 tiles
  // Ensure ContileIntegration._fetchSites is working populate _sites and initilize TelemetryUtility
  let fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 3);
  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  // 2.  Set the Numbus pref to 3, confirm previous count still used.
  const nimbusPocketStub = sandbox.stub(
    NimbusFeatures.pocketNewtab,
    "getVariable"
  );
  setNimbusVariablesForNumTiles(nimbusPocketStub, 3);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  // 3.  Confirm the new count is applied when data pulled from Contile, 3 tiles displayed
  fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 3);
  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 3);

  expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 3,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(
  async function test_update_telemetry_fields_if_dismissed_brands_list_is_updated() {
    let sandbox = sinon.createSandbox();
    info(
      "if the user dismisses a brand, that dismissed tile shoudl be represented in the next ping."
    );
    let { feed, fetchStub } = prepFeed(
      getTopSitesFeedForTest(sandbox),
      sandbox
    );

    fetchStub.resolves({
      ok: true,
      status: 200,
      headers: new Map([
        ["cache-control", "private, max-age=859, stale-if-error=10463"],
      ]),
      json: () =>
        Promise.resolve({
          tiles: [
            {
              url: "https://foo.com",
              image_url: "images/foo.png",
              click_url: "https://www.foo-click.com",
              impression_url: "https://www.foo-impression.com",
              name: "foo",
            },
            {
              url: "https://www.brand2.com",
              image_url: "images/brand2-com.png",
              click_url: "https://www.brand2-click.com",
              impression_url: "https://www.brand2-impression.com",
              name: "brand2",
            },
            {
              url: "https://www.brand3.com",
              image_url: "images/brand3-com.png",
              click_url: "https://www.brand3-click.com",
              impression_url: "https://www.brand3-impression.com",
              name: "brand3",
            },
          ],
        }),
    });
    Services.prefs.setStringPref(
      TOP_SITES_BLOCKED_SPONSORS_PREF,
      `["foo","bar"]`
    );

    let fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 2);

    await feed._readDefaults();
    await feed.getLinksWithDefaults(false);

    Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

    let expectedResult = {
      sponsoredTilesReceived: [
        {
          advertiser: "foo",
          provider: "amp",
          display_position: null,
          display_fail_reason: "dismissed",
        },
        {
          advertiser: "brand2",
          provider: "amp",
          display_position: 1,
          display_fail_reason: null,
        },
        {
          advertiser: "brand3",
          provider: "amp",
          display_position: 2,
          display_fail_reason: null,
        },
      ],
    };
    Assert.equal(
      Glean.topsites.sponsoredTilesReceived.testGetValue(),
      JSON.stringify(expectedResult)
    );

    Services.prefs.setStringPref(
      TOP_SITES_BLOCKED_SPONSORS_PREF,
      `["foo","bar","brand2"]`
    );

    fetched = await feed._contile._fetchSites();
    Assert.ok(fetched);
    Assert.equal(feed._contile.sites.length, 1);

    await feed._readDefaults();
    await feed.getLinksWithDefaults(false);

    Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

    expectedResult = {
      sponsoredTilesReceived: [
        {
          advertiser: "foo",
          provider: "amp",
          display_position: null,
          display_fail_reason: "dismissed",
        },
        {
          advertiser: "brand2",
          provider: "amp",
          display_position: null,
          display_fail_reason: "dismissed",
        },
        {
          advertiser: "brand3",
          provider: "amp",
          display_position: 1,
          display_fail_reason: null,
        },
      ],
    };
    Assert.equal(
      Glean.topsites.sponsoredTilesReceived.testGetValue(),
      JSON.stringify(expectedResult)
    );

    Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
    sandbox.restore();
  }
);

add_task(async function test_sponsoredTilesReceived_not_set() {
  let sandbox = sinon.createSandbox();
  info("sponsoredTilesReceived should be empty if tiles service returns 204");
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({ ok: true, status: 204 });

  const fetched = await feed._contile._fetchSites();
  Assert.ok(!fetched);
  Assert.ok(!feed._contile.sites.length);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);
  let expectedResult = { sponsoredTilesReceived: [] };

  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});

add_task(async function test_telemetry_data_updates() {
  let sandbox = sinon.createSandbox();
  info("sponsoredTilesReceived should update when new tiles received.");
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://foo.com",
            image_url: "images/foo.png",
            click_url: "https://www.foo-click.com",
            impression_url: "https://www.foo-impression.com",
            name: "foo",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brand3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "brand3",
          },
        ],
      }),
  });
  Services.prefs.setStringPref(
    TOP_SITES_BLOCKED_SPONSORS_PREF,
    `["foo","bar"]`
  );

  const fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "foo",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://foo.com",
            image_url: "images/foo.png",
            click_url: "https://www.foo-click.com",
            impression_url: "https://www.foo-impression.com",
            name: "foo",
          },
          {
            url: "https://bar.com",
            image_url: "images/bar-com.png",
            click_url: "https://www.bar-click.com",
            impression_url: "https://www.bar-impression.com",
            name: "bar",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brand3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "brand3",
          },
        ],
      }),
  });

  await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 1);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "foo",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
      {
        advertiser: "bar",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
  sandbox.restore();
});

add_task(async function test_reset_telemetry_data() {
  let sandbox = sinon.createSandbox();
  info(
    "sponsoredTilesReceived should be cleared when no tiles received and cache refreshed."
  );
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://foo.com",
            image_url: "images/foo.png",
            click_url: "https://www.foo-click.com",
            impression_url: "https://www.foo-impression.com",
            name: "foo",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
          {
            url: "https://www.brand3.com",
            image_url: "images/brand3-com.png",
            click_url: "https://www.brand3-click.com",
            impression_url: "https://www.brand3-impression.com",
            name: "test3",
          },
        ],
      }),
  });
  Services.prefs.setStringPref(
    TOP_SITES_BLOCKED_SPONSORS_PREF,
    `["foo","bar"]`
  );

  let fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "foo",
        provider: "amp",
        display_position: null,
        display_fail_reason: "dismissed",
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand3",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );

  fetchStub.resolves({ ok: true, status: 204 });

  fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.ok(!feed._contile.sites.length);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);

  expectedResult = { sponsoredTilesReceived: [] };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  Services.prefs.clearUserPref(TOP_SITES_BLOCKED_SPONSORS_PREF);
  sandbox.restore();
});

add_task(async function test_set_telemetry_for_moz_sales_tiles() {
  let sandbox = sinon.createSandbox();
  let { feed, fetchStub } = prepFeed(getTopSitesFeedForTest(sandbox), sandbox);

  sandbox.stub(feed, "fetchDiscoveryStreamSpocs").returns([
    {
      label: "MozSales Title",
      title: "MozSales Title",
      url: "https://mozsale.net",
      sponsored_position: 1,
      partner: "moz-sales",
    },
  ]);

  fetchStub.resolves({
    ok: true,
    status: 200,
    headers: new Map([
      ["cache-control", "private, max-age=859, stale-if-error=10463"],
    ]),
    json: () =>
      Promise.resolve({
        tiles: [
          {
            url: "https://www.brand1.com",
            image_url: "images/brand1-com.png",
            click_url: "https://www.brand1-click.com",
            impression_url: "https://www.brand1-impression.com",
            name: "brand1",
          },
          {
            url: "https://www.brand2.com",
            image_url: "images/brand2-com.png",
            click_url: "https://www.brand2-click.com",
            impression_url: "https://www.brand2-impression.com",
            name: "brand2",
          },
        ],
      }),
  });
  const fetched = await feed._contile._fetchSites();
  Assert.ok(fetched);
  Assert.equal(feed._contile.sites.length, 2);

  await feed._readDefaults();
  await feed.getLinksWithDefaults(false);

  Assert.equal(Glean.topsites.sponsoredTilesConfigured.testGetValue(), 2);
  let expectedResult = {
    sponsoredTilesReceived: [
      {
        advertiser: "brand1",
        provider: "amp",
        display_position: 1,
        display_fail_reason: null,
      },
      {
        advertiser: "brand2",
        provider: "amp",
        display_position: 2,
        display_fail_reason: null,
      },
      {
        advertiser: "mozsales title",
        provider: "moz-sales",
        display_position: null,
        display_fail_reason: "oversold",
      },
    ],
  };
  Assert.equal(
    Glean.topsites.sponsoredTilesReceived.testGetValue(),
    JSON.stringify(expectedResult)
  );
  sandbox.restore();
});
