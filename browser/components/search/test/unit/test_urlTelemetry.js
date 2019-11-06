/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SearchTelemetry } = ChromeUtils.import(
  "resource:///modules/SearchTelemetry.jsm"
);

const TESTS = [
  {
    title: "Google search access point",
    trackingUrl:
      "https://www.google.com/search?q=test&ie=utf-8&oe=utf-8&client=firefox-b-1-ab",
    expectedSearchCountEntry: "google.in-content:sap:firefox-b-1-ab",
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
      "https://www.google.com/search?source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE",
    expectedSearchCountEntry: "google.in-content:organic:none",
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
  },
  {
    title: "Bing organic",
    trackingUrl:
      "https://www.bing.com/search?q=test&qs=n&form=QBLH&sp=-1&pq=&sc=0-0&sk=&cvid=CVID_VALUE",
    expectedSearchCountEntry: "bing.in-content:organic:none",
  },
  {
    title: "DuckDuckGo search access point",
    trackingUrl: "https://duckduckgo.com/?q=test&t=ffab",
    expectedSearchCountEntry: "duckduckgo.in-content:sap:ffab",
  },
  {
    title: "DuckDuckGo organic",
    trackingUrl: "https://duckduckgo.com/?q=test&t=hi&ia=news",
    expectedSearchCountEntry: "duckduckgo.in-content:organic:hi",
  },
  {
    title: "Baidu search access point",
    trackingUrl: "https://www.baidu.com/baidu?wd=test&tn=monline_7_dg&ie=utf-8",
    expectedSearchCountEntry: "baidu.in-content:sap:monline_7_dg",
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
      "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&rsv_idx=1&ch=&tn=baidu&bar=&wd=test&rn=&oq=&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn",
    expectedSearchCountEntry: "baidu.in-content:organic:baidu",
  },
];

add_task(async function test_parsing_search_urls() {
  for (const test of TESTS) {
    info(`Running ${test.title}`);
    SearchTelemetry.updateTrackingStatus({}, test.trackingUrl);
    const hs = Services.telemetry
      .getKeyedHistogramById("SEARCH_COUNTS")
      .snapshot();
    Assert.ok(hs);
    Assert.ok(
      test.expectedSearchCountEntry in hs,
      "The histogram must contain the correct key"
    );
  }
});
