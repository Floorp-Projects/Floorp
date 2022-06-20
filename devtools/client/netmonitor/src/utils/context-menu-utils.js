/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The default format for the content copied to the
 *  clipboard when the `Copy Value` option is selected.
 */
function baseCopyFormatter({ name, value, object, hasChildren }) {
  if (hasChildren) {
    return baseCopyAllFormatter({ [name]: value });
  }
  return `${value}`;
}

/**
 * The default format for the content copied to the
 *  clipboard when the `Copy All` option is selected.
 * @param {Object} object The whole data object
 */
function baseCopyAllFormatter(object) {
  return JSON.stringify(object, null, "\t");
}

module.exports = {
  contextMenuFormatters: {
    baseCopyFormatter,
    baseCopyAllFormatter,
  },
};
