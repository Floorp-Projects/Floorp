/* this source code form is subject to the terms of the mozilla public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A mapping of header names to external documentation. Any header included
 * here will show a "Learn More" link alongside it.
 */

"use strict";

var URL_DOMAIN = "https://developer.mozilla.org";
const URL_PATH = "/en-US/docs/Web/HTTP/Headers/";
const URL_PARAMS =
  "?utm_source=mozilla&utm_medium=devtools-netmonitor&utm_campaign=default";

var SUPPORTED_HEADERS = [
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
  "Cache-Control",
  "Connection",
  "Content-Disposition",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-Security-Policy",
  "Content-Security-Policy-Report-Only",
  "Content-Type",
  "Cookie",
  "Cookie2",
  "DNT",
  "Date",
  "ETag",
  "Expires",
  "From",
  "Host",
  "If-Match",
  "If-Modified-Since",
  "If-None-Match",
  "If-Range",
  "If-Unmodified-Since",
  "Keep-Alive",
  "Last-Modified",
  "Location",
  "Origin",
  "Pragma",
  "Public-Key-Pins",
  "Public-Key-Pins-Report-Only",
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
  "Warning",
  "X-Content-Type-Options",
  "X-DNS-Prefetch-Control",
  "X-Frame-Options",
  "X-XSS-Protection"
];

/**
 * Get the MDN URL for the specified header
 *
 * @param {string} Name of the header
 * The baseURL to use.
 *
 * @return {string}
 * The MDN URL for the header, or null if not available.
 */
exports.getURL = (header) => {
  const lowerCaseHeader = header.toLowerCase();

  let matchingHeader = SUPPORTED_HEADERS.find(supportedHeader => {
    return lowerCaseHeader === supportedHeader.toLowerCase();
  });

  if (!matchingHeader) {
    return null;
  }

  return URL_DOMAIN + URL_PATH + matchingHeader + URL_PARAMS;
};

/**
 * Use a different domain for the URLs. Used only for testing.
 *
 * @param {string} domain
 * The domain to use.
 */
exports.setDomain = (domain) => {
  URL_DOMAIN = domain;
};

/**
 * Use a different list of supported headers. Used only for testing.
 *
 * @param {array} headers
 * The supported headers to use.
 */
exports.setSupportedHeaders = (headers) => {
  SUPPORTED_HEADERS = headers;
};
