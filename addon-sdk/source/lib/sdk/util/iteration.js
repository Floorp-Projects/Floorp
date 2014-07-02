/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "experimental"
};

// This is known as @@iterator in the ES6 spec. In builds that have ES6
// Symbols, use Symbol.iterator; otherwise use the legacy method name,
// "@@iterator".
const JS_HAS_SYMBOLS = typeof Symbol === "function";
exports.iteratorSymbol = JS_HAS_SYMBOLS ? Symbol.iterator : "@@iterator";

// An adaptor that, given an object that is iterable with for-of, is
// suitable for being bound to __iterator__ in order to make the object
// iterable in the same way via for-in.
function forInIterator() {
    for (let item of this)
        yield item;
}

exports.forInIterator = forInIterator;
