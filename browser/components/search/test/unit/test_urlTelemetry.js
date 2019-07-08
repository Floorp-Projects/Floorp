/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SearchTelemetry } = ChromeUtils.import(
  "resource:///modules/SearchTelemetry.jsm"
);

add_task(async function test_parsing_search_urls() {
  let hs;
  // Google search access point.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.google.com/search?q=test&ie=utf-8&oe=utf-8&client=firefox-b-1-ab"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "google.in-content:sap:firefox-b-1-ab" in hs,
    "The histogram must contain the correct key"
  );

  // Google search access point follow-on.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.google.com/search?client=firefox-b-1-ab&ei=EI_VALUE&q=test2&oq=test2&gs_l=GS_L_VALUE"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "google.in-content:sap-follow-on:firefox-b-1-ab" in hs,
    "The histogram must contain the correct key"
  );

  // Google organic.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.google.com/search?source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "google.in-content:organic:none" in hs,
    "The histogram must contain the correct key"
  );

  // Google organic UK.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.google.co.uk/search?source=hp&ei=EI_VALUE&q=test&oq=test&gs_l=GS_L_VALUE"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "google.in-content:organic:none" in hs,
    "The histogram must contain the correct key"
  );

  // Yahoo organic.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://search.yahoo.com/search?p=test&fr=yfp-t&fp=1&toggle=1&cop=mss&ei=UTF-8"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "yahoo.in-content:organic:none" in hs,
    "The histogram must contain the correct key"
  );

  // Yahoo organic UK.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://uk.search.yahoo.com/search?p=test&fr=yfp-t&fp=1&toggle=1&cop=mss&ei=UTF-8"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "yahoo.in-content:organic:none" in hs,
    "The histogram must contain the correct key"
  );

  // Bing search access point.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.bing.com/search?q=test&pc=MOZI&form=MOZLBR"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "bing.in-content:sap:MOZI" in hs,
    "The histogram must contain the correct key"
  );

  // Bing organic.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.bing.com/search?q=test&qs=n&form=QBLH&sp=-1&pq=&sc=0-0&sk=&cvid=CVID_VALUE"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "bing.in-content:organic:none" in hs,
    "The histogram must contain the correct key"
  );

  // DuckDuckGo search access point.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://duckduckgo.com/?q=test&t=ffab"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "duckduckgo.in-content:sap:ffab" in hs,
    "The histogram must contain the correct key"
  );

  // DuckDuckGo organic.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://duckduckgo.com/?q=test&t=hi&ia=news"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "duckduckgo.in-content:organic:hi" in hs,
    "The histogram must contain the correct key"
  );

  // Baidu search access point.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.baidu.com/baidu?wd=test&tn=monline_7_dg&ie=utf-8"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "baidu.in-content:sap:monline_7_dg" in hs,
    "The histogram must contain the correct key"
  );

  // Baidu search access point follow-on.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&tn=monline_7_dg&wd=test2&oq=test&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn&rsv_enter=1&rsv_sug3=2&rsv_sug2=0&inputT=227&rsv_sug4=397"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "baidu.in-content:sap-follow-on:monline_7_dg" in hs,
    "The histogram must contain the correct key"
  );

  // Baidu organic.
  SearchTelemetry.updateTrackingStatus(
    {},
    "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&rsv_idx=1&ch=&tn=baidu&bar=&wd=test&rn=&oq=&rsv_pq=RSV_PQ_VALUE&rsv_t=RSV_T_VALUE&rqlang=cn"
  );
  hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
  Assert.ok(hs);
  Assert.ok(
    "baidu.in-content:organic:baidu" in hs,
    "The histogram must contain the correct key"
  );
});
