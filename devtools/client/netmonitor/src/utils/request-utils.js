/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */

"use strict";

const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};

/**
 * Extracts any urlencoded form data sections (e.g. "?foo=bar&baz=42") from a
 * POST request.
 *
 * @param {object} headers - the "requestHeaders".
 * @param {object} uploadHeaders - the "requestHeadersFromUploadStream".
 * @param {object} postData - the "requestPostData".
 * @return {array} a promise list that is resolved with the extracted form data.
 */
async function getFormDataSections(headers, uploadHeaders, postData, getLongString) {
  let formDataSections = [];

  let requestHeaders = headers.headers;
  let payloadHeaders = uploadHeaders ? uploadHeaders.headers : [];
  let allHeaders = [...payloadHeaders, ...requestHeaders];

  let contentTypeHeader = allHeaders.find(e => {
    return e.name.toLowerCase() == "content-type";
  });

  let contentTypeLongString = contentTypeHeader ? contentTypeHeader.value : "";

  let contentType = await getLongString(contentTypeLongString);

  if (contentType.includes("x-www-form-urlencoded")) {
    let postDataLongString = postData.postData.text;
    let text = await getLongString(postDataLongString);

    for (let section of text.split(/\r\n|\r|\n/)) {
      // Before displaying it, make sure this section of the POST data
      // isn't a line containing upload stream headers.
      if (payloadHeaders.every(header => !section.startsWith(header.name))) {
        formDataSections.push(section);
      }
    }
  }

  return formDataSections;
}

/**
 * Fetch headers full content from actor server
 *
 * @param {object} headers - a object presents headers data
 * @return {object} a headers object with updated content payload
 */
async function fetchHeaders(headers, getLongString) {
  for (let { value } of headers.headers) {
    headers.headers.value = await getLongString(value);
  }
  return headers;
}

/**
 * Form a data: URI given a mime type, encoding, and some text.
 *
 * @param {string} mimeType - mime type
 * @param {string} encoding - encoding to use; if not set, the
 *                            text will be base64-encoded.
 * @param {string} text - text of the URI.
 * @return {string} a data URI
 */
function formDataURI(mimeType, encoding, text) {
  if (!encoding) {
    encoding = "base64";
    text = btoa(unescape(encodeURIComponent(text)));
  }
  return "data:" + mimeType + ";" + encoding + "," + text;
}

/**
 * Write out a list of headers into a chunk of text
 *
 * @param {array} headers - array of headers info { name, value }
 * @return {string} list of headers in text format
 */
function writeHeaderText(headers) {
  return headers.map(({name, value}) => name + ": " + value).join("\n");
}

/**
 * Convert a string into unicode if string is valid.
 * If there is a malformed URI sequence, it returns input string.
 *
 * @param {string} url - a string
 * @return {string} unicode string
 */
function decodeUnicodeUrl(string) {
  try {
    return decodeURIComponent(string);
  } catch (err) {
    // Ignore error and return input string directly.
  }
  return string;
}

/**
 * Helper for getting an abbreviated string for a mime type.
 *
 * @param {string} mimeType - mime type
 * @return {string} abbreviated mime type
 */
function getAbbreviatedMimeType(mimeType) {
  if (!mimeType) {
    return "";
  }
  let abbrevType = (mimeType.split(";")[0].split("/")[1] || "").split("+")[0];
  return CONTENT_MIME_TYPE_ABBREVIATIONS[abbrevType] || abbrevType;
}

/**
 * Helpers for getting the last portion of a url.
 * For example helper returns "basename" from http://domain.com/path/basename
 * If basename portion is empty, it returns the url pathname.
 *
 * @param {string} url - url string
 * @return {string} unicode basename of a url
 */
function getUrlBaseName(url) {
  const pathname = (new URL(url)).pathname;
  return decodeUnicodeUrl(
    pathname.replace(/\S*\//, "") || pathname || "/");
}

/**
 * Helpers for getting the query portion of a url.
 *
 * @param {string} url - url string
 * @return {string} unicode query of a url
 */
function getUrlQuery(url) {
  return (new URL(url)).search.replace(/^\?/, "");
}

/**
 * Helpers for getting unicode name and query portions of a url.
 *
 * @param {string} url - url string
 * @return {string} unicode basename and query portions of a url
 */
function getUrlBaseNameWithQuery(url) {
  return getUrlBaseName(url) + decodeUnicodeUrl((new URL(url)).search);
}

/**
 * Helpers for getting unicode hostname portion of an URL.
 *
 * @param {string} url - url string
 * @return {string} unicode hostname of a url
 */
function getUrlHostName(url) {
  return decodeUnicodeUrl((new URL(url)).hostname);
}

/**
 * Helpers for getting unicode host portion of an URL.
 *
 * @param {string} url - url string
 * @return {string} unicode host of a url
 */
function getUrlHost(url) {
  return decodeUnicodeUrl((new URL(url)).host);
}

/**
 * Helpers for getting the shceme portion of a url.
 * For example helper returns "http" from http://domain.com/path/basename
 *
 * @param {string} url - url string
 * @return {string} string scheme of a url
 */
function getUrlScheme(url) {
  return (new URL(url)).protocol.replace(":", "").toLowerCase();
}

/**
 * Extract several details fields from a URL at once.
 */
function getUrlDetails(url) {
  let baseNameWithQuery = getUrlBaseNameWithQuery(url);
  let host = getUrlHost(url);
  let hostname = getUrlHostName(url);
  let unicodeUrl = decodeUnicodeUrl(url);
  let scheme = getUrlScheme(url);

  // Mark local hosts specially, where "local" is  as defined in the W3C
  // spec for secure contexts.
  // http://www.w3.org/TR/powerful-features/
  //
  //  * If the name falls under 'localhost'
  //  * If the name is an IPv4 address within 127.0.0.0/8
  //  * If the name is an IPv6 address within ::1/128
  //
  // IPv6 parsing is a little sloppy; it assumes that the address has
  // been validated before it gets here.
  let isLocal = hostname.match(/(.+\.)?localhost$/) ||
                hostname.match(/^127\.\d{1,3}\.\d{1,3}\.\d{1,3}/) ||
                hostname.match(/\[[0:]+1\]/);

  return {
    baseNameWithQuery,
    host,
    scheme,
    unicodeUrl,
    isLocal
  };
}

/**
 * Parse a url's query string into its components
 *
 * @param {string} query - query string of a url portion
 * @return {array} array of query params { name, value }
 */
function parseQueryString(query) {
  if (!query) {
    return null;
  }

  return query.replace(/^[?&]/, "").split("&").map(e => {
    let param = e.split("=");
    return {
      name: param[0] ? decodeUnicodeUrl(param[0]) : "",
      value: param[1] ? decodeUnicodeUrl(param[1]) : "",
    };
  });
}

/**
 * Parse a string of formdata sections into its components
 *
 * @param {string} sections - sections of formdata joined by &
 * @return {array} array of formdata params { name, value }
 */
function parseFormData(sections) {
  if (!sections) {
    return null;
  }

  return sections.replace(/^&/, "").split("&").map(e => {
    let param = e.split("=");
    return {
      name: param[0] ? decodeUnicodeUrl(param[0]) : "",
      value: param[1] ? decodeUnicodeUrl(param[1]) : "",
    };
  });
}

/**
 * Reduces an IP address into a number for easier sorting
 *
 * @param {string} ip - IP address to reduce
 * @return {number} the number representing the IP address
 */
function ipToLong(ip) {
  if (!ip) {
    // Invalid IP
    return -1;
  }

  let base;
  let octets = ip.split(".");

  if (octets.length === 4) { // IPv4
    base = 10;
  } else if (ip.includes(":")) { // IPv6
    let numberOfZeroSections = 8 - ip.replace(/^:+|:+$/g, "").split(/:+/g).length;
    octets = ip
      .replace("::", `:${"0:".repeat(numberOfZeroSections)}`)
      .replace(/^:|:$/g, "")
      .split(":");
    base = 16;
  } else { // Invalid IP
    return -1;
  }
  return octets.map((val, ix, arr) => {
    return parseInt(val, base) * Math.pow(256, (arr.length - 1) - ix);
  }).reduce((sum, val) => {
    return sum + val;
  }, 0);
}

/**
 * Compare two objects on a subset of their properties
 */
function propertiesEqual(props, item1, item2) {
  return item1 === item2 || props.every(p => item1[p] === item2[p]);
}

/**
 * Calculate the start time of a request, which is the time from start
 * of 1st request until the start of this request.
 *
 * Without a firstRequestStartedMillis argument the wrong time will be returned.
 * However, it can be omitted when comparing two start times and neither supplies
 * a firstRequestStartedMillis.
 */
function getStartTime(item, firstRequestStartedMillis = 0) {
  return item.startedMillis - firstRequestStartedMillis;
}

/**
 * Calculate the end time of a request, which is the time from start
 * of 1st request until the end of this response.
 *
 * Without a firstRequestStartedMillis argument the wrong time will be returned.
 * However, it can be omitted when comparing two end times and neither supplies
 * a firstRequestStartedMillis.
 */
function getEndTime(item, firstRequestStartedMillis = 0) {
  let { startedMillis, totalTime } = item;
  return startedMillis + totalTime - firstRequestStartedMillis;
}

/**
 * Calculate the response time of a request, which is the time from start
 * of 1st request until the beginning of download of this response.
 *
 * Without a firstRequestStartedMillis argument the wrong time will be returned.
 * However, it can be omitted when comparing two response times and neither supplies
 * a firstRequestStartedMillis.
 */
function getResponseTime(item, firstRequestStartedMillis = 0) {
  let { startedMillis, totalTime, eventTimings = { timings: {} } } = item;
  return startedMillis + totalTime - firstRequestStartedMillis -
    eventTimings.timings.receive;
}

/**
 * Format the protocols used by the request.
 */
function getFormattedProtocol(item) {
  let { httpVersion = "", responseHeaders = { headers: [] } } = item;
  let protocol = [httpVersion];
  responseHeaders.headers.some(h => {
    if (h.hasOwnProperty("name") && h.name.toLowerCase() === "x-firefox-spdy") {
      protocol.push(h.value);
      return true;
    }
    return false;
  });
  return protocol.join("+");
}

module.exports = {
  getFormDataSections,
  fetchHeaders,
  formDataURI,
  writeHeaderText,
  decodeUnicodeUrl,
  getAbbreviatedMimeType,
  getEndTime,
  getFormattedProtocol,
  getResponseTime,
  getStartTime,
  getUrlBaseName,
  getUrlBaseNameWithQuery,
  getUrlDetails,
  getUrlHost,
  getUrlHostName,
  getUrlQuery,
  getUrlScheme,
  parseQueryString,
  parseFormData,
  propertiesEqual,
  ipToLong,
};
