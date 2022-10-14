/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for doc-utils

"use strict";

function run_test() {
  const { require } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/Loader.sys.mjs"
  );
  const MDN_URL = "https://developer.mozilla.org/docs/";
  const GTM_PARAMS_NM =
    "?utm_source=mozilla" +
    "&utm_medium=devtools-netmonitor&utm_campaign=default";
  const GTM_PARAMS_WC =
    "?utm_source=mozilla" +
    "&utm_medium=devtools-webconsole&utm_campaign=default";
  const USER_DOC_URL = "https://firefox-source-docs.mozilla.org/devtools-user/";

  const {
    getHeadersURL,
    getHTTPStatusCodeURL,
    getNetMonitorTimingsURL,
    getPerformanceAnalysisURL,
    getFilterBoxURL,
  } = require("resource://devtools/client/netmonitor/src/utils/doc-utils.js");

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
    `${USER_DOC_URL}network_monitor/request_details/#network-monitor-request-details-timings-tab`
  );

  equal(
    getPerformanceAnalysisURL(),
    `${USER_DOC_URL}network_monitor/performance_analysis/`
  );

  equal(
    getFilterBoxURL(),
    `${USER_DOC_URL}network_monitor/request_list/#filtering-by-properties`
  );
}
