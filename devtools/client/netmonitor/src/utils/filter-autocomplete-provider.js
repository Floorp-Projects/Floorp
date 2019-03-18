/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { FILTER_FLAGS, SUPPORTED_HTTP_CODES } = require("../constants");

/**
 * Generates a value for the given filter
 * ie. if flag = status-code, will generate "200" from the given request item.
 * For flags related to cookies, it might generate an array based on the request
 * ie. ["cookie-name-1", "cookie-name-2", ...]
 *
 * @param {string} flag - flag specified in filter, ie. "status-code"
 * @param {object} request - Network request item
 * @return {string|Array} - The output is a string or an array based on the request
 */
function getAutocompleteValuesForFlag(flag, request) {
  let values = [];
  let { responseCookies = { cookies: [] } } = request;
  responseCookies = responseCookies.cookies || responseCookies;

  switch (flag) {
    case "status-code":
      // Sometimes status comes as Number
      values.push(String(request.status));
      break;
    case "scheme":
      values.push(request.urlDetails.scheme);
      break;
    case "domain":
      values.push(request.urlDetails.host);
      break;
    case "remote-ip":
      values.push(request.remoteAddress);
      break;
    case "cause":
      values.push(request.cause.type);
      break;
    case "mime-type":
      values.push(request.mimeType.replace(/;.+/, ""));
      break;
    case "set-cookie-name":
      values = responseCookies.map(c => c.name);
      break;
    case "set-cookie-value":
      values = responseCookies.map(c => c.value);
      break;
    case "set-cookie-domain":
      values = responseCookies.map(c => c.hasOwnProperty("domain") ?
          c.domain : request.urlDetails.host);
      break;
    case "is":
      values = ["cached", "from-cache", "running"];
      break;
    case "has-response-header":
      // Some requests not having responseHeaders..?
      values = request.responseHeaders ?
        request.responseHeaders.headers.map(h => h.name) : [];
      break;
    case "protocol":
      values.push(request.httpVersion);
      break;
    case "method":
    default:
      values.push(request[flag]);
  }

  return values;
}

/**
 * For a given lastToken passed ie. "is:", returns an array of populated flag
 * values for consumption in autocompleteProvider
 * ie. ["is:cached", "is:running", "is:from-cache"]
 *
 * @param {string} lastToken - lastToken parsed from filter input, ie "is:"
 * @param {object} requests - List of requests from which values are generated
 * @return {Array} - array of autocomplete values
 */
function getLastTokenFlagValues(lastToken, requests) {
  // The last token must be a string like "method:GET" or "method:", Any token
  // without a ":" cant be used to parse out flag values
  if (!lastToken.includes(":")) {
    return [];
  }

  // Parse out possible flag from lastToken
  let [flag, typedFlagValue] = lastToken.split(":");
  let isNegativeFlag = false;

  // Check if flag is used with negative match
  if (flag.startsWith("-")) {
    flag = flag.slice(1);
    isNegativeFlag = true;
  }

  // Flag is some random string, return
  if (!FILTER_FLAGS.includes(flag)) {
    return [];
  }

  let values = [];
  for (const request of requests) {
    values.push(...getAutocompleteValuesForFlag(flag, request));
  }
  values = [...new Set(values)];

  return values
    .filter(value => value)
    .filter(value => {
      if (typedFlagValue && value) {
        const lowerTyped = typedFlagValue.toLowerCase();
        const lowerValue = value.toLowerCase();
        return lowerValue.includes(lowerTyped) && lowerValue !== lowerTyped;
      }
      return typeof value !== "undefined" && value !== "" && value !== "undefined";
    })
    .sort()
    .map(value => isNegativeFlag ? `-${flag}:${value}` : `${flag}:${value}`);
}

/**
 * Generates an autocomplete list for the search-box for network monitor
 *
 * It expects an entire string of the searchbox ie "is:cached pr".
 * The string is then tokenized into "is:cached" and "pr"
 *
 * @param {string} filter - The entire search string of the search box
 * @param {object} requests - Iteratable object of requests displayed
 * @return {Array} - The output is an array of objects as below
 * [{value: "is:cached protocol", displayValue: "protocol"}[, ...]]
 * `value` is used to update the search-box input box for given item
 * `displayValue` is used to render the autocomplete list
 */
function autocompleteProvider(filter, requests) {
  if (!filter) {
    return [];
  }

  const negativeAutocompleteList = FILTER_FLAGS.map((item) => `-${item}`);
  const baseList = [...FILTER_FLAGS, ...negativeAutocompleteList]
    .map((item) => `${item}:`);

  // The last token is used to filter the base autocomplete list
  const tokens = filter.split(/\s+/g);
  const lastToken = tokens[tokens.length - 1];
  const previousTokens = tokens.slice(0, tokens.length - 1);

  // Autocomplete list is not generated for empty lastToken
  if (!lastToken) {
    return [];
  }

  let autocompleteList;
  const availableValues = getLastTokenFlagValues(lastToken, requests);
  if (availableValues.length > 0) {
    autocompleteList = availableValues;
  } else {
    const isNegativeFlag = lastToken.startsWith("-");

    // Stores list of HTTP codes that starts with value of lastToken
    const filteredStatusCodes = SUPPORTED_HTTP_CODES.filter((item) => {
      item = isNegativeFlag ? item.substr(1) : item;
      return item.toLowerCase().startsWith(lastToken.toLowerCase());
    });

    if (filteredStatusCodes.length > 0) {
      // Shows an autocomplete list of "status-code" values from filteredStatusCodes
      autocompleteList = isNegativeFlag ?
        filteredStatusCodes.map((item) => `-status-code:${item}`) :
        filteredStatusCodes.map((item) => `status-code:${item}`);
    } else {
      // Shows an autocomplete list of values from baseList
      // that starts with value of lastToken
      autocompleteList = baseList
        .filter((item) => {
          return item.toLowerCase().startsWith(lastToken.toLowerCase())
            && item.toLowerCase() !== lastToken.toLowerCase();
        });
    }
  }

  return autocompleteList
    .sort()
    .map(item => ({
      value: [...previousTokens, item].join(" "),
      displayValue: item,
    }));
}

module.exports = {
  autocompleteProvider,
};
