/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disclaimer: Some of the functions in this module implement APIs from
// Jeremy Ashkenas's http://underscorejs.org/ library and all credits for
// those goes to him.

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { defer, remit, delay, debounce,
        throttle } = require("./functional/concurrent");
const { method, invoke, partial, curry, compose, wrap, identity, memoize, once,
        cache, complement, constant, when, apply, flip, field, query,
        isInstance, chainable, is, isnt } = require("./functional/core");

exports.defer = defer;
exports.remit = remit;
exports.delay = delay;
exports.debounce = debounce;
exports.throttle = throttle;

exports.method = method;
exports.invoke = invoke;
exports.partial = partial;
exports.curry = curry;
exports.compose = compose;
exports.wrap = wrap;
exports.identity = identity;
exports.memoize = memoize;
exports.once = once;
exports.cache = cache;
exports.complement = complement;
exports.constant = constant;
exports.when = when;
exports.apply = apply;
exports.flip = flip;
exports.field = field;
exports.query = query;
exports.isInstance = isInstance;
exports.chainable = chainable;
exports.is = is;
exports.isnt = isnt;
