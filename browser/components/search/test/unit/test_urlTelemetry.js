/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

const TESTS = [
  {
    title: "Google search access point",
    trackingUrl:
      "https://www.google.com/search?q=test&ie=utf-8&oe=utf-8&client=firefox-b-1-ab",
    expectedSearchCountEntry: "google.in-content:sap:firefox-b-1-ab",
    expectedAdKey: "google:sap",
    adUrls: [
      "https://www.googleadservices.com/aclk=foobar",
      "https://www.googleadservices.com/pagead/aclk=foobar",
      "https://www.google.com/aclk=foobar",
      "https://www.google.com/pagead/aclk=foobar",
    ],
    nonAdUrls: [
      "https://www.googleadservices.com/?aclk=foobar",
      "https://www.googleadservices.com/bar",
      "https://www.google.com/image",
    ],
  },
  {
    title: "Google search access point follow-on",
    trackingUrl:
      "https://www.google.com/search?client=firefox-b-1-ab&ei=EI_VALUE&q=test2&oq=test2&gs_l=GS_L_VALUE",
    expectedSearchCountEntry: "google.in-content:sap-follow-on:firefox-b-1-ab",
  },
  {
    title: "Google organic",
    trackingUrl:
      "https://www.google.com/search?client=firefox-b-d-invalid&source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE",
    expectedSearchCountEntry: "google.in-content:organic:other",
    expectedAdKey: "google:organic",
    adUrls: ["https://www.googleadservices.com/aclk=foobar"],
    nonAdUrls: ["https://www.googleadservices.com/?aclk=foobar"],
  },
  {
    title: "Google organic no code",
    trackingUrl:
      "https://www.google.com/search?source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE",
    expectedSearchCountEntry: "google.in-content:organic:none",
    expectedAdKey: "google:organic",
    adUrls: ["https://www.googleadservices.com/aclk=foobar"],
    nonAdUrls: ["https://www.googleadservices.com/?aclk=foobar"],
  },
  {
    title: "Google organic UK",
    trackingUrl:
      "https://www.google.co.uk/search?source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE",
    expectedSearchCountEntry: "google.in-content:organic:none",
  },
  {
    title: "Yahoo organic",
    trackingUrl:
      "https://search.yahoo.com/search?p=test&fr=yfp-t&fp=1&toggle=1&cop=mss&ei=UTF-8",
    expectedSearchCountEntry: "yahoo.in-content:organic:none",
  },
  {
    title: "Yahoo organic UK",
    trackingUrl:
      "https://uk.search.yahoo.com/search?p=test&fr=yfp-t&fp=1&toggle=1&cop=mss&ei=UTF-8",
    expectedSearchCountEntry: "yahoo.in-content:organic:none",
  },
  {
    title: "Bing search access point",
    trackingUrl: "https://www.bing.com/search?q=test&pc=MOZI&form=MOZLBR",
    expectedSearchCountEntry: "bing.in-content:sap:MOZI",
    expectedAdKey: "bing:sap",
    adUrls: [
      "https://www.bing.com/aclick?ld=foo",
      "https://www.bing.com/aclk?ld=foo",
    ],
    nonAdUrls: [
      "https://www.bing.com/fd/ls/ls.gif?IG=foo",
      "https://www.bing.com/fd/ls/l?IG=bar",
      "https://www.bing.com/aclook?",
      "https://www.bing.com/fd/ls/GLinkPingPost.aspx?IG=baz&url=%2Fvideos%2Fsearch%3Fq%3Dfoo",
      "https://www.bing.com/fd/ls/GLinkPingPost.aspx?IG=bar&url=https%3A%2F%2Fwww.bing.com%2Faclick",
      "https://www.bing.com/fd/ls/GLinkPingPost.aspx?IG=bar&url=https%3A%2F%2Fwww.bing.com%2Faclk",
    ],
  },
  {
    setUp() {
      Services.cookies.removeAll();
      Services.cookies.add(
        "www.bing.com",
        "/",
        "SRCHS",
        "PC=MOZI",
        false,
        false,
        false,
        Date.now() + 1000 * 60 * 60,
        {},
        Ci.nsICookie.SAMESITE_NONE,
        Ci.nsICookie.SCHEME_HTTPS
      );
    },
    tearDown() {
      Services.cookies.removeAll();
    },
    title: "Bing search access point follow-on",
    trackingUrl:
      "https://www.bing.com/search?q=test&qs=n&form=QBRE&sp=-1&pq=&sc=0-0&sk=&cvid=CVID_VALUE",
    expectedSearchCountEntry: "bing.in-content:sap-follow-on:MOZI",
  },
  {
    title: "Bing organic",
    trackingUrl: "https://www.bing.com/search?q=test&pc=MOZIfoo&form=MOZLBR",
    expectedSearchCountEntry: "bing.in-content:organic:other",
    expectedAdKey: "bing:organic",
    adUrls: ["https://www.bing.com/aclick?ld=foo"],
    nonAdUrls: ["https://www.bing.com/fd/ls/ls.gif?IG=foo"],
  },
  {
    title: "Bing organic no code",
    trackingUrl:
      "https://www.bing.com/search?q=test&qs=n&form=QBLH&sp=-1&pq=&sc=0-0&sk=&cvid=CVID_VALUE",
    expectedSearchCountEntry: "bing.in-content:organic:none",
    expectedAdKey: "bing:organic",
    adUrls: ["https://www.bing.com/aclick?ld=foo"],
    nonAdUrls: ["https://www.bing.com/fd/ls/ls.gif?IG=foo"],
  },
  {
    title: "DuckDuckGo search access point",
    trackingUrl: "https://duckduckgo.com/?q=test&t=ffab",
    expectedSearchCountEntry: "duckduckgo.in-content:sap:ffab",
    expectedAdKey: "duckduckgo:sap",
    adUrls: [
      "https://duckduckgo.com/y.js?ad_provider=foo",
      "https://duckduckgo.com/y.js?f=bar&ad_provider=foo",
      "https://www.amazon.co.uk/foo?tag=duckduckgo-ffab-uk-32-xk",
    ],
    nonAdUrls: [
      "https://duckduckgo.com/?q=foo&t=ffab&ia=images&iax=images",
      "https://duckduckgo.com/y.js?ifu=foo",
      "https://improving.duckduckgo.com/t/bar",
    ],
  },
  {
    title: "DuckDuckGo organic",
    trackingUrl: "https://duckduckgo.com/?q=test&t=hi&ia=news",
    expectedSearchCountEntry: "duckduckgo.in-content:organic:other",
    expectedAdKey: "duckduckgo:organic",
    adUrls: ["https://duckduckgo.com/y.js?ad_provider=foo"],
    nonAdUrls: ["https://duckduckgo.com/?q=foo&t=ffab&ia=images&iax=images"],
  },
  {
    title: "DuckDuckGo expected organic code",
    trackingUrl: "https://duckduckgo.com/?q=test&t=h_&ia=news",
    expectedSearchCountEntry: "duckduckgo.in-content:organic:none",
    expectedAdKey: "duckduckgo:organic",
    adUrls: ["https://duckduckgo.com/y.js?ad_provider=foo"],
    nonAdUrls: ["https://duckduckgo.com/?q=foo&t=ffab&ia=images&iax=images"],
  },
  {
    title: "DuckDuckGo expected organic code 2",
    trackingUrl: "https://duckduckgo.com/?q=test&t=hz&ia=news",
    expectedSearchCountEntry: "duckduckgo.in-content:organic:none",
    expectedAdKey: "duckduckgo:organic",
    adUrls: ["https://duckduckgo.com/y.js?ad_provider=foo"],
    nonAdUrls: ["https://duckduckgo.com/?q=foo&t=ffab&ia=images&iax=images"],
  },
  {
    title: "DuckDuckGo organic no code",
    trackingUrl: "https://duckduckgo.com/?q=test&ia=news",
    expectedSearchCountEntry: "duckduckgo.in-content:organic:none",
    expectedAdKey: "duckduckgo:organic",
    adUrls: ["https://duckduckgo.com/y.js?ad_provider=foo"],
    nonAdUrls: ["https://duckduckgo.com/?q=foo&t=ffab&ia=images&iax=images"],
  },
  {
    title: "Baidu search access point",
    trackingUrl: "https://www.baidu.com/baidu?wd=test&tn=monline_7_dg&ie=utf-8",
    expectedSearchCountEntry: "baidu.in-content:sap:monline_7_dg",
    expectedAdKey: "baidu:sap",
    adUrls: ["https://www.baidu.com/baidu.php?url=encoded"],
    nonAdUrls: ["https://www.baidu.com/link?url=encoded"],
  },
  {
    title: "Baidu search access point follow-on",
    trackingUrl:
      "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&tn=monline_7_dg&wd=test2&oq=test&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn&rsv_enter=1&rsv_sug3=2&rsv_sug2=0&inputT=227&rsv_sug4=397",
    expectedSearchCountEntry: "baidu.in-content:sap-follow-on:monline_7_dg",
  },
  {
    title: "Baidu organic",
    trackingUrl:
      "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&rsv_idx=1&ch=&tn=baidu&bar=&wd=test&rn=&oq&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn",
    expectedSearchCountEntry: "baidu.in-content:organic:other",
  },
  {
    title: "Baidu organic no code",
    trackingUrl:
      "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&rsv_idx=1&ch=&bar=&wd=test&rn=&oq&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn",
    expectedSearchCountEntry: "baidu.in-content:organic:none",
  },
];

/**
 * This function is primarily for testing the Ad URL regexps that are triggered
 * when a URL is clicked on. These regexps are also used for the `with_ads`
 * probe. However, we test the ad_clicks route as that is easier to hit.
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
      !("browser.search.ad_clicks" in scalars),
      "Should not have recorded an ad click"
    );
  } else {
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "browser.search.ad_clicks",
      expectedAdKey,
      1
    );
  }
}

do_get_profile();

add_task(async function setup() {
  Services.prefs.setBoolPref(SearchUtils.BROWSER_SEARCH_PREF + "log", true);
  await SearchSERPTelemetry.init();
  sinon.stub(BrowserSearchTelemetry, "shouldRecordSearchCount").returns(true);
});

add_task(async function test_parsing_search_urls() {
  for (const test of TESTS) {
    info(`Running ${test.title}`);
    if (test.setUp) {
      test.setUp();
    }
    SearchSERPTelemetry.updateTrackingStatus(
      {
        getTabBrowser: () => {},
      },
      test.trackingUrl
    );
    let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
    let snapshot = histogram.snapshot();
    Assert.ok(snapshot);
    Assert.ok(
      test.expectedSearchCountEntry in snapshot,
      "The histogram must contain the correct key"
    );

    if ("adUrls" in test) {
      for (const adUrl of test.adUrls) {
        await testAdUrlClicked(test.trackingUrl, adUrl, test.expectedAdKey);
      }
      for (const nonAdUrls of test.nonAdUrls) {
        await testAdUrlClicked(test.trackingUrl, nonAdUrls);
      }
    }

    if (test.tearDown) {
      test.tearDown();
    }
    histogram.clear();
  }
});
