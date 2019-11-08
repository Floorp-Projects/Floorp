/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function toCamelCase(text) {
  return text.replace(/-([a-z])/gi, (str, group) => {
    return group.toUpperCase();
  });
}

function toSnakeCase(text) {
  return text.replace(/[a-z]([A-Z])/g, (str, group) => {
    return `${str.charAt(0)}-${group.toLowerCase()}`;
  });
}

module.exports = {
  toCamelCase,
  toSnakeCase,
};
