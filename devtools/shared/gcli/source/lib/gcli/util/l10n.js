/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools-shared/locale/gcli.properties");

/*
 * Not supported when embedded - we"re doing things the Mozilla way not the
 * require.js way.
 */
exports.registerStringsSource = function (modulePath) {
  throw new Error("registerStringsSource is not available in mozilla");
};

exports.unregisterStringsSource = function (modulePath) {
  throw new Error("unregisterStringsSource is not available in mozilla");
};

exports.lookupSwap = function (key, swaps) {
  throw new Error("lookupSwap is not available in mozilla");
};

exports.lookupPlural = function (key, ord, swaps) {
  throw new Error("lookupPlural is not available in mozilla");
};

exports.getPreferredLocales = function () {
  return [ "root" ];
};

/** @see lookup() in lib/gcli/util/l10n.js */
exports.lookup = function (key) {
  try {
    // Our memory leak hunter walks reachable objects trying to work out what
    // type of thing they are using object.constructor.name. If that causes
    // problems then we can avoid the unknown-key-exception with the following:
    /*
    if (key === "constructor") {
      return { name: "l10n-mem-leak-defeat" };
    }
    */

    return L10N.getStr(key);
  } catch (ex) {
    console.error("Failed to lookup ", key, ex);
    return key;
  }
};

/** @see propertyLookup in lib/gcli/util/l10n.js */
exports.propertyLookup = new Proxy({}, {
  get: function (rcvr, name) {
    return exports.lookup(name);
  }
});

/** @see lookupFormat in lib/gcli/util/l10n.js */
exports.lookupFormat = function (key, swaps) {
  try {
    return L10N.getFormatStr(key, ...swaps);
  } catch (ex) {
    console.error("Failed to format ", key, ex);
    return key;
  }
};
