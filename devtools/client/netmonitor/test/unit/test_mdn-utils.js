/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for mdn-utils

"use strict";

function run_test() {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const MDN_URL = "https://developer.mozilla.org/docs/";
  const GTM_PARAMS_NM =
    "?utm_source=mozilla" +
    "&utm_medium=devtools-netmonitor&utm_campaign=default";
  const GTM_PARAMS_WC =
    "?utm_source=mozilla" +
    "&utm_medium=devtools-webconsole&utm_campaign=default";

  const {
    getHeadersURL,
    getHTTPStatusCodeURL,
    getNetMonitorTimingsURL,
    getPerformanceAnalysisURL,
    getFilterBoxURL,
  } = require("devtools/client/netmonitor/src/utils/mdn-utils");

  info("Checking for supported headers");
  equal(
    getHeadersURL("Accept"),
    `${MDN_URL}Web/HTTP/Headers/Accept${GTM_PARAMS_NM}`
  );
  info("Checking for unsupported headers");
  equal(getHeadersURL("Width"), null);

  info("Checking for supported status code");
  equal(
    getHTTPStatusCodeURL("200", "webconsole"),
    `${MDN_URL}Web/HTTP/Status/200${GTM_PARAMS_WC}`
  );
  info("Checking for unsupported status code");
  equal(
    getHTTPStatusCodeURL("999", "webconsole"),
    `${MDN_URL}Web/HTTP/Status${GTM_PARAMS_WC}`
  );

  equal(
    getNetMonitorTimingsURL(),
    `${MDN_URL}Tools/Network_Monitor/request_details${GTM_PARAMS_NM}#Timings`
  );

  equal(
    getPerformanceAnalysisURL(),
    `${MDN_URL}Tools/Network_Monitor/Performance_analysis${GTM_PARAMS_NM}`
  );

  equal(
    getFilterBoxURL(),
    `${MDN_URL}Tools/Network_Monitor/request_list` +
      `${GTM_PARAMS_NM}#Filtering_by_properties`
  );
}
