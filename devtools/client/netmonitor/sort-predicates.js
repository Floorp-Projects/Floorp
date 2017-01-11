/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAbbreviatedMimeType,
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

function compareValues(first, second) {
  if (first === second) {
    return 0;
  }
  return first > second ? 1 : -1;
}

function waterfall(first, second) {
  const result = compareValues(first.startedMillis, second.startedMillis);
  return result || compareValues(first.id, second.id);
}

function status(first, second) {
  const result = compareValues(first.status, second.status);
  return result || waterfall(first, second);
}

function method(first, second) {
  const result = compareValues(first.method, second.method);
  return result || waterfall(first, second);
}

function file(first, second) {
  const firstUrl = first.urlDetails.baseNameWithQuery.toLowerCase();
  const secondUrl = second.urlDetails.baseNameWithQuery.toLowerCase();
  const result = compareValues(firstUrl, secondUrl);
  return result || waterfall(first, second);
}

function domain(first, second) {
  const firstDomain = first.urlDetails.host.toLowerCase();
  const secondDomain = second.urlDetails.host.toLowerCase();
  const result = compareValues(firstDomain, secondDomain);
  return result || waterfall(first, second);
}

function cause(first, second) {
  const firstCause = first.cause.type;
  const secondCause = second.cause.type;
  const result = compareValues(firstCause, secondCause);
  return result || waterfall(first, second);
}

function type(first, second) {
  const firstType = getAbbreviatedMimeType(first.mimeType).toLowerCase();
  const secondType = getAbbreviatedMimeType(second.mimeType).toLowerCase();
  const result = compareValues(firstType, secondType);
  return result || waterfall(first, second);
}

function transferred(first, second) {
  const result = compareValues(first.transferredSize, second.transferredSize);
  return result || waterfall(first, second);
}

function size(first, second) {
  const result = compareValues(first.contentSize, second.contentSize);
  return result || waterfall(first, second);
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
