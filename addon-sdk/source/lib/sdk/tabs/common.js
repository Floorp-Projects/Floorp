/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { validateOptions } = require("../deprecated/api-utils");
const { data } = require("../self");

function Options(options) {
  if ('string' === typeof options)
    options = { url: options };

  return validateOptions(options, {
    url: {
      is: ["string"],
      map: (v) => v ? data.url(v) : v
    },
    inBackground: {
      map: Boolean,
      is: ["undefined", "boolean"]
    },
    isPinned: { is: ["undefined", "boolean"] },
    isPrivate: { is: ["undefined", "boolean"] },
    inNewWindow: { is: ["undefined", "boolean"] },
    onOpen: { is: ["undefined", "function"] },
    onClose: { is: ["undefined", "function"] },
    onReady: { is: ["undefined", "function"] },
    onLoad: { is: ["undefined", "function"] },
    onPageShow: { is: ["undefined", "function"] },
    onActivate: { is: ["undefined", "function"] },
    onDeactivate: { is: ["undefined", "function"] }
  });
}
exports.Options = Options;
