/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAbbreviatedMimeType,
  getUrlBaseNameWithQuery,
  getUrlHost,
  loadCauseString,
} = require("./request-utils");

/**
 * Predicates used when sorting items.
 *
 * @param object first
 *        The first item used in the comparison.
 * @param object second
 *        The second item used in the comparison.
 * @return number
 *         <0 to sort first to a lower index than second
 *         =0 to leave first and second unchanged with respect to each other
 *         >0 to sort second to a lower index than first
 */

function waterfall(first, second) {
  return first.startedMillis - second.startedMillis;
}

function status(first, second) {
  return first.status == second.status
         ? first.startedMillis - second.startedMillis
         : first.status - second.status;
}

function method(first, second) {
  if (first.method == second.method) {
    return first.startedMillis - second.startedMillis;
  }
  return first.method > second.method ? 1 : -1;
}

function file(first, second) {
  let firstUrl = getUrlBaseNameWithQuery(first.url).toLowerCase();
  let secondUrl = getUrlBaseNameWithQuery(second.url).toLowerCase();
  if (firstUrl == secondUrl) {
    return first.startedMillis - second.startedMillis;
  }
  return firstUrl > secondUrl ? 1 : -1;
}

function domain(first, second) {
  let firstDomain = getUrlHost(first.url).toLowerCase();
  let secondDomain = getUrlHost(second.url).toLowerCase();
  if (firstDomain == secondDomain) {
    return first.startedMillis - second.startedMillis;
  }
  return firstDomain > secondDomain ? 1 : -1;
}

function cause(first, second) {
  let firstCause = loadCauseString(first.cause.type);
  let secondCause = loadCauseString(second.cause.type);
  if (firstCause == secondCause) {
    return first.startedMillis - second.startedMillis;
  }
  return firstCause > secondCause ? 1 : -1;
}

function type(first, second) {
  let firstType = getAbbreviatedMimeType(first.mimeType).toLowerCase();
  let secondType = getAbbreviatedMimeType(second.mimeType).toLowerCase();
  if (firstType == secondType) {
    return first.startedMillis - second.startedMillis;
  }
  return firstType > secondType ? 1 : -1;
}

function transferred(first, second) {
  return first.transferredSize - second.transferredSize;
}

function size(first, second) {
  return first.contentSize - second.contentSize;
}

exports.Sorters = {
  status,
  method,
  file,
  domain,
  cause,
  type,
  transferred,
  size,
  waterfall,
};
