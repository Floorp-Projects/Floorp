/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */

"use strict";

const { KeyCodes } = require("devtools/client/shared/keycodes");
const { Task } = require("devtools/shared/task");

/**
 * Helper method to get a wrapped function which can be bound to as
 * an event listener directly and is executed only when data-key is
 * present in event.target.
 *
 * @param {function} callback - function to execute execute when data-key
 *                              is present in event.target.
 * @param {bool} onlySpaceOrReturn - flag to indicate if callback should only
 *                                   be called when the space or return button
 *                                   is pressed
 * @return {function} wrapped function with the target data-key as the first argument
 *                    and the event as the second argument.
 */
function getKeyWithEvent(callback, onlySpaceOrReturn) {
  return function (event) {
    let key = event.target.getAttribute("data-key");
    let filterKeyboardEvent = !onlySpaceOrReturn ||
                              event.keyCode === KeyCodes.DOM_VK_SPACE ||
                              event.keyCode === KeyCodes.DOM_VK_RETURN;

    if (key && filterKeyboardEvent) {
      callback.call(null, key);
    }
  };
}

/**
 * Extracts any urlencoded form data sections (e.g. "?foo=bar&baz=42") from a
 * POST request.
 *
 * @param {object} headers - the "requestHeaders".
 * @param {object} uploadHeaders - the "requestHeadersFromUploadStream".
 * @param {object} postData - the "requestPostData".
 * @param {function} getString - callback to retrieve a string from a LongStringGrip.
 * @return {array} a promise list that is resolved with the extracted form data.
 */
const getFormDataSections = Task.async(function* (headers, uploadHeaders, postData,
                                                    getString) {
  let formDataSections = [];

  let requestHeaders = headers.headers;
  let payloadHeaders = uploadHeaders ? uploadHeaders.headers : [];
  let allHeaders = [...payloadHeaders, ...requestHeaders];

  let contentTypeHeader = allHeaders.find(e => {
    return e.name.toLowerCase() == "content-type";
  });

  let contentTypeLongString = contentTypeHeader ? contentTypeHeader.value : "";

  let contentType = yield getString(contentTypeLongString);

  if (contentType.includes("x-www-form-urlencoded")) {
    let postDataLongString = postData.postData.text;
    let text = yield getString(postDataLongString);

    for (let section of text.split(/\r\n|\r|\n/)) {
      // Before displaying it, make sure this section of the POST data
      // isn't a line containing upload stream headers.
      if (payloadHeaders.every(header => !section.startsWith(header.name))) {
        formDataSections.push(section);
      }
    }
  }

  return formDataSections;
});

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
    text = btoa(text);
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
  return (mimeType.split(";")[0].split("/")[1] || "").split("+")[0];
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
  return decodeUnicodeUrl((new URL(url)).search.replace(/^\?/, ""));
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
 * Extract several details fields from a URL at once.
 */
function getUrlDetails(url) {
  let baseNameWithQuery = getUrlBaseNameWithQuery(url);
  let host = getUrlHost(url);
  let hostname = getUrlHostName(url);
  let unicodeUrl = decodeUnicodeUrl(url);

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

module.exports = {
  getKeyWithEvent,
  getFormDataSections,
  formDataURI,
  writeHeaderText,
  decodeUnicodeUrl,
  getAbbreviatedMimeType,
  getUrlBaseName,
  getUrlQuery,
  getUrlBaseNameWithQuery,
  getUrlHostName,
  getUrlHost,
  getUrlDetails,
  parseQueryString,
};
