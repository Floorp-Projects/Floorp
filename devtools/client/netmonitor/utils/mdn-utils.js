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
 * Get the MDN URL for the specified header.
 *
 * @param {string} header Name of the header for the baseURL to use.
 *
 * @return {string} The MDN URL for the header, or null if not available.
 */
function getHeadersURL(header) {
  const lowerCaseHeader = header.toLowerCase();
  let idx = SUPPORTED_HEADERS.findIndex(item =>
    item.toLowerCase() === lowerCaseHeader);
  return idx > -1 ?
    `https://developer.mozilla.org/docs/Web/HTTP/Headers/${SUPPORTED_HEADERS[idx]}?utm_source=mozilla&utm_medium=devtools-netmonitor&utm_campaign=default` : null;
}

module.exports = {
  getHeadersURL,
};
