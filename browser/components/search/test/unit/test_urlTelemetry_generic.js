/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPTelemetryUtils: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/www\.example\.com\/search/,
    queryParamNames: ["q"],
    codeParamName: "abc",
    taggedCodes: ["ff", "tb"],
    expectedOrganicCodes: ["baz"],
    organicCodes: ["foo"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/www\.example\.com\/ad2/],
    shoppingTab: {
      regexp: "&site=shop",
    },
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
  {
    telemetryId: "example2",
    searchPageRegexp: /^https:\/\/www\.example2\.com\/search/,
    queryParamNames: ["a", "q"],
    codeParamName: "abc",
    taggedCodes: ["ff", "tb"],
    expectedOrganicCodes: ["baz"],
    organicCodes: ["foo"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/www\.example\.com\/ad2/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

const TESTS = [
  {
    title: "Tagged search",
    trackingUrl: "https://www.example.com/search?q=test&abc=ff",
    expectedSearchCountEntry: "example:tagged:ff",
    expectedAdKey: "example:tagged",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "true",
      partner_code: "ff",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Tagged search with shopping",
    trackingUrl: "https://www.example.com/search?q=test&abc=ff&site=shop",
    expectedSearchCountEntry: "example:tagged:ff",
    expectedAdKey: "example:tagged",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "true",
      partner_code: "ff",
      is_shopping_page: "true",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Tagged follow-on",
    trackingUrl: "https://www.example.com/search?q=test&abc=tb&a=next",
    expectedSearchCountEntry: "example:tagged-follow-on:tb",
    expectedAdKey: "example:tagged-follow-on",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "true",
      partner_code: "tb",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Organic search matched code",
    trackingUrl: "https://www.example.com/search?q=test&abc=foo",
    expectedSearchCountEntry: "example:organic:foo",
    expectedAdKey: "example:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "false",
      partner_code: "foo",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Organic search non-matched code",
    trackingUrl: "https://www.example.com/search?q=test&abc=ff123",
    expectedSearchCountEntry: "example:organic:other",
    expectedAdKey: "example:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "false",
      partner_code: "other",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Organic search non-matched code 2",
    trackingUrl: "https://www.example.com/search?q=test&abc=foo123",
    expectedSearchCountEntry: "example:organic:other",
    expectedAdKey: "example:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "false",
      partner_code: "other",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Organic search expected organic matched code",
    trackingUrl: "https://www.example.com/search?q=test&abc=baz",
    expectedSearchCountEntry: "example:organic:none",
    expectedAdKey: "example:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "false",
      partner_code: "",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Organic search no codes",
    trackingUrl: "https://www.example.com/search?q=test",
    expectedSearchCountEntry: "example:organic:none",
    expectedAdKey: "example:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example",
      tagged: "false",
      partner_code: "",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
  {
    title: "Different engines using the same adUrl",
    trackingUrl: "https://www.example2.com/search?q=test",
    expectedSearchCountEntry: "example2:organic:none",
    expectedAdKey: "example2:organic",
    adUrls: ["https://www.example.com/ad2"],
    nonAdUrls: ["https://www.example.com/ad3"],
    impression: {
      provider: "example2",
      tagged: "false",
      partner_code: "",
      is_shopping_page: "false",
      is_private: "false",
      shopping_tab_displayed: "false",
      source: "unknown",
    },
  },
];

/**
 * This function is primarily for testing the Ad URL regexps that are triggered
 * when a URL is clicked on. These regexps are also used for the `withads`
 * probe. However, we test the adclicks route as that is easier to hit.
 *
 * @param {string} serpUrl
 *   The url to simulate where the page the click came from.
 * @param {string} adUrl
 *   The ad url to simulate being clicked.
 * @param {string} [expectedAdKey]
 *   The expected key to be logged for the scalar. Omit if no scalar should be
 *   logged.
 */
async function testAdUrlClicked(serpUrl, adUrl, expectedAdKey) {
  info(`Testing Ad URL: ${adUrl}`);
  let channel = NetUtil.newChannel({
    uri: NetUtil.newURI(adUrl),
    triggeringPrincipal: Services.scriptSecurityManager.createContentPrincipal(
      NetUtil.newURI(serpUrl),
      {}
    ),
    loadUsingSystemPrincipal: true,
  });
  SearchSERPTelemetry._contentHandler.observeActivity(
    channel,
    Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION,
    Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE
  );
  // Since the content handler takes a moment to allow the channel information
  // to settle down, wait the same amount of time here.
  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  if (!expectedAdKey) {
    Assert.ok(
      !("browser.search.adclicks.unknown" in scalars),
      "Should not have recorded an ad click"
    );
  } else {
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "browser.search.adclicks.unknown",
      expectedAdKey,
      1
    );
  }
}

do_get_profile();

add_task(async function setup() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "serpEventTelemetry.enabled",
    true
  );
  Services.fog.initializeFOG();
  await SearchSERPTelemetry.init();
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  sinon.stub(BrowserSearchTelemetry, "shouldRecordSearchCount").returns(true);
  // There is no concept of browsing in unit tests, so assume in tests that we
  // are not in private browsing mode. We have browser tests that check when
  // private browsing is used.
  sinon.stub(PrivateBrowsingUtils, "isBrowserPrivate").returns(false);
});

add_task(async function test_parsing_search_urls() {
  for (const test of TESTS) {
    info(`Running ${test.title}`);
    if (test.setUp) {
      test.setUp();
    }
    let browser = {
      getTabBrowser: () => {},
    };
    SearchSERPTelemetry.updateTrackingStatus(browser, test.trackingUrl);
    SearchSERPTelemetry.reportPageImpression(
      {
        url: test.trackingUrl,
        shoppingTabDisplayed: false,
      },
      browser
    );
    let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "browser.search.content.unknown",
      test.expectedSearchCountEntry,
      1
    );

    if ("adUrls" in test) {
      for (const adUrl of test.adUrls) {
        await testAdUrlClicked(test.trackingUrl, adUrl, test.expectedAdKey);
      }
      for (const nonAdUrls of test.nonAdUrls) {
        await testAdUrlClicked(test.trackingUrl, nonAdUrls);
      }
    }

    let recordedEvents = Glean.serp.impression.testGetValue();

    Assert.equal(
      recordedEvents.length,
      1,
      "should only see one impression event"
    );

    // To allow deep equality.
    test.impression.impression_id = recordedEvents[0].extra.impression_id;
    Assert.deepEqual(recordedEvents[0].extra, test.impression);

    if (test.tearDown) {
      test.tearDown();
    }

    // We need to clear Glean events so they don't accumulate for each iteration.
    Services.fog.testResetFOG();
  }
});
