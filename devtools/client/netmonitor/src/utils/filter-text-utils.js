/*
 * Copyright (c) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"use strict";

const { FILTER_FLAGS, SUPPORTED_HTTP_CODES } = require("../constants");
const { getFormattedIPAndPort } = require("./format-utils");
const { getUnicodeUrl } = require("devtools/client/shared/unicode-url");

/*
  The function `parseFilters` is from:
  https://github.com/ChromeDevTools/devtools-frontend/

  front_end/network/FilterSuggestionBuilder.js#L138-L163
  Commit f340aefd7ec9b702de9366a812288cfb12111fce
*/

function parseFilters(query) {
  const flags = [];
  const text = [];
  const parts = query.split(/\s+/);

  for (const part of parts) {
    if (!part) {
      continue;
    }
    const colonIndex = part.indexOf(":");
    if (colonIndex === -1) {
      const isNegative = part.startsWith("-");
      // Stores list of HTTP codes that starts with value of lastToken
      const filteredStatusCodes = SUPPORTED_HTTP_CODES.filter(item => {
        item = isNegative ? item.substr(1) : item;
        return item.toLowerCase().startsWith(part.toLowerCase());
      });

      if (filteredStatusCodes.length > 0) {
        flags.push({
          type: "status-code", // a standard key before a colon
          value: isNegative ? part.substring(1) : part,
          isNegative,
        });
        continue;
      }

      // Value of lastToken is just text that does not correspond to status codes
      text.push(part);
      continue;
    }
    let key = part.substring(0, colonIndex);
    const negative = key.startsWith("-");
    if (negative) {
      key = key.substring(1);
    }
    if (!FILTER_FLAGS.includes(key)) {
      text.push(part);
      continue;
    }
    let value = part.substring(colonIndex + 1);
    value = processFlagFilter(key, value);
    flags.push({
      type: key,
      value,
      negative,
    });
  }

  return { text, flags };
}

function processFlagFilter(type, value) {
  switch (type) {
    case "regexp":
      return value;
    case "size":
    case "transferred":
    case "larger-than":
    case "transferred-larger-than":
      let multiplier = 1;
      if (value.endsWith("k")) {
        multiplier = 1024;
        value = value.substring(0, value.length - 1);
      } else if (value.endsWith("m")) {
        multiplier = 1024 * 1024;
        value = value.substring(0, value.length - 1);
      }
      const quantity = Number(value);
      if (isNaN(quantity)) {
        return null;
      }
      return quantity * multiplier;
    default:
      return value.toLowerCase();
  }
}

function isFlagFilterMatch(item, { type, value, negative }) {
  if (value == null) {
    return false;
  }

  // Ensures when filter token is exactly a flag ie. "remote-ip:", all values are shown
  if (value.length < 1) {
    return true;
  }

  let match = true;
  let { responseCookies = { cookies: [] } } = item;
  responseCookies = responseCookies.cookies || responseCookies;

  const matchers = {
    "status-code": () => item.status && item.status.toString() === value,
    method: () => item.method.toLowerCase() === value,
    protocol: () => {
      const protocol = item.httpVersion;
      return typeof protocol === "string"
        ? protocol.toLowerCase().includes(value)
        : false;
    },
    domain: () => item.urlDetails.host.toLowerCase().includes(value),
    "remote-ip": () => {
      const data = getFormattedIPAndPort(item.remoteAddress, item.remotePort);
      return data ? data.toLowerCase().includes(value) : false;
    },
    "has-response-header": () => {
      if (typeof item.responseHeaders === "object") {
        const { headers } = item.responseHeaders;
        return headers.findIndex(h => h.name.toLowerCase() === value) > -1;
      }
      return false;
    },
    cause: () => {
      const causeType = item.cause.type;
      return typeof causeType === "string"
        ? causeType.toLowerCase().includes(value)
        : false;
    },
    transferred: () => {
      if (item.fromCache) {
        return false;
      }
      return isSizeMatch(value, item.transferredSize);
    },
    size: () => isSizeMatch(value, item.contentSize),
    "larger-than": () => item.contentSize > value,
    "transferred-larger-than": () => {
      if (item.fromCache) {
        return false;
      }
      return item.transferredSize > value;
    },
    "mime-type": () => item.mimeType.includes(value),
    is: () => {
      if (value === "from-cache" || value === "cached") {
        return item.fromCache || item.status === "304";
      }
      if (value === "running") {
        return !item.status;
      }
      return match;
    },
    scheme: () => item.urlDetails.scheme === value,
    regexp: () => {
      try {
        const pattern = new RegExp(value);
        return pattern.test(item.url);
      } catch (e) {
        return false;
      }
    },
    "set-cookie-domain": () => {
      if (responseCookies.length > 0) {
        const host = item.urlDetails.host;
        const i = responseCookies.findIndex(c => {
          const domain = c.hasOwnProperty("domain") ? c.domain : host;
          return domain.includes(value);
        });
        return i > -1;
      }
      return false;
    },
    "set-cookie-name": () =>
      responseCookies.findIndex(c => c.name.toLowerCase().includes(value)) > -1,
    "set-cookie-value": () =>
      responseCookies.findIndex(c => c.value.toLowerCase().includes(value)) >
      -1,
  };

  const matcher = matchers[type];
  if (matcher) {
    match = matcher();
  }

  return negative ? !match : match;
}

function isSizeMatch(value, size) {
  return value >= size - size / 10 && value <= size + size / 10;
}

function isTextFilterMatch({ url }, text) {
  const lowerCaseUrl = getUnicodeUrl(url).toLowerCase();
  let lowerCaseText = text.toLowerCase();
  const textLength = text.length;
  // Support negative filtering
  if (text.startsWith("-") && textLength > 1) {
    lowerCaseText = lowerCaseText.substring(1, textLength);
    return !lowerCaseUrl.includes(lowerCaseText);
  }

  // no text is a positive match
  return !text || lowerCaseUrl.includes(lowerCaseText);
}

function isFreetextMatch(item, text) {
  if (!text) {
    return true;
  }

  const filters = parseFilters(text);
  let match = true;

  for (const textFilter of filters.text) {
    match = match && isTextFilterMatch(item, textFilter);
  }

  for (const flagFilter of filters.flags) {
    match = match && isFlagFilterMatch(item, flagFilter);
  }

  return match;
}

module.exports = {
  isFreetextMatch,
};
