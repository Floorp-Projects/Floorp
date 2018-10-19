/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A mapping of header names to external documentation. Any header included
 * here will show a MDN link alongside it.
 */
const SUPPORTED_HEADERS = [
  "Accept",
  "Accept-Charset",
  "Accept-Encoding",
  "Accept-Language",
  "Accept-Ranges",
  "Access-Control-Allow-Credentials",
  "Access-Control-Allow-Headers",
  "Access-Control-Allow-Methods",
  "Access-Control-Allow-Origin",
  "Access-Control-Expose-Headers",
  "Access-Control-Max-Age",
  "Access-Control-Request-Headers",
  "Access-Control-Request-Method",
  "Age",
  "Allow",
  "Authorization",
  "Cache-Control",
  "Connection",
  "Content-Disposition",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-Range",
  "Content-Security-Policy",
  "Content-Security-Policy-Report-Only",
  "Content-Type",
  "Cookie",
  "Cookie2",
  "DNT",
  "Date",
  "ETag",
  "Expect",
  "Expires",
  "Forwarded",
  "From",
  "Host",
  "If-Match",
  "If-Modified-Since",
  "If-None-Match",
  "If-Range",
  "If-Unmodified-Since",
  "Keep-Alive",
  "Large-Allocation",
  "Last-Modified",
  "Location",
  "Origin",
  "Pragma",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "Public-Key-Pins",
  "Public-Key-Pins-Report-Only",
  "Range",
  "Referer",
  "Referrer-Policy",
  "Retry-After",
  "Server",
  "Set-Cookie",
  "Set-Cookie2",
  "Strict-Transport-Security",
  "TE",
  "Tk",
  "Trailer",
  "Transfer-Encoding",
  "Upgrade-Insecure-Requests",
  "User-Agent",
  "Vary",
  "Via",
  "WWW-Authenticate",
  "Warning",
  "X-Content-Type-Options",
  "X-DNS-Prefetch-Control",
  "X-Forwarded-For",
  "X-Forwarded-Host",
  "X-Forwarded-Proto",
  "X-Frame-Options",
  "X-XSS-Protection"
];

/**
 * A mapping of HTTP status codes to external documentation. Any code included
 * here will show a MDN link alongside it.
 */
const SUPPORTED_HTTP_CODES = [
    "100",
    "101",
    "200",
    "201",
    "202",
    "203",
    "204",
    "205",
    "206",
    "300",
    "301",
    "302",
    "303",
    "304",
    "307",
    "308",
    "400",
    "401",
    "403",
    "404",
    "405",
    "406",
    "407",
    "408",
    "409",
    "410",
    "411",
    "412",
    "413",
    "414",
    "415",
    "416",
    "417",
    "418",
    "426",
    "428",
    "429",
    "431",
    "451",
    "500",
    "501",
    "502",
    "503",
    "504",
    "505",
    "511"
];

const MDN_URL = "https://developer.mozilla.org/docs/";
const MDN_STATUS_CODES_LIST_URL = `${MDN_URL}Web/HTTP/Status`;
const getGAParams = (panelId = "netmonitor") => {
  return `?utm_source=mozilla&utm_medium=devtools-${panelId}&utm_campaign=default`;
};

/**
 * Get the MDN URL for the specified header.
 *
 * @param {string} header Name of the header for the baseURL to use.
 *
 * @return {string} The MDN URL for the header, or null if not available.
 */
function getHeadersURL(header) {
  const lowerCaseHeader = header.toLowerCase();
  const idx = SUPPORTED_HEADERS.findIndex(item =>
    item.toLowerCase() === lowerCaseHeader);
  return idx > -1 ?
    `${MDN_URL}Web/HTTP/Headers/${SUPPORTED_HEADERS[idx] + getGAParams()}` : null;
}

/**
 * Get the MDN URL for the specified HTTP status code.
 *
 * @param {string} HTTP status code for the baseURL to use.
 *
 * @return {string} The MDN URL for the HTTP status code, or null if not available.
 */
function getHTTPStatusCodeURL(statusCode, panelId) {
  return (
    SUPPORTED_HTTP_CODES.includes(statusCode)
      ? `${MDN_URL}Web/HTTP/Status/${statusCode}`
      : MDN_STATUS_CODES_LIST_URL
    ) + getGAParams(panelId);
}

/**
 * Get the MDN URL of the Timings tag for Network Monitor.
 *
 * @return {string} the MDN URL of the Timings tag for Network Monitor.
 */
function getNetMonitorTimingsURL() {
  return `${MDN_URL}Tools/Network_Monitor${getGAParams()}#Timings`;
}

/**
 * Get the MDN URL for Performance Analysis
 *
 * @return {string} The MDN URL for the documentation of Performance Analysis.
 */
function getPerformanceAnalysisURL() {
  return `${MDN_URL}Tools/Network_Monitor${getGAParams()}#Performance_analysis`;
}

/**
 * Get the MDN URL for Filter box
 *
 * @return {string} The MDN URL for the documentation of Filter box.
 */
function getFilterBoxURL() {
  return `${MDN_URL}Tools/Network_Monitor${getGAParams()}#Filtering_by_properties`;
}

module.exports = {
  getHeadersURL,
  getHTTPStatusCodeURL,
  getNetMonitorTimingsURL,
  getPerformanceAnalysisURL,
  getFilterBoxURL,
};
