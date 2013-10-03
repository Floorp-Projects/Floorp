/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "experimental"
};

// This is known as @@iterator in the ES6 spec.  Until it is bound to
// some well-known name, find the @@iterator object by expecting it as
// the first property accessed on a for-of iterable.
const iteratorSymbol = (function() {
  try {
    for (var _ of Proxy.create({get: function(_, name) { throw name; } }))
      break;
  } catch (name) {
    return name;
  }
  throw new TypeError;
})();

exports.iteratorSymbol = iteratorSymbol;

// An adaptor that, given an object that is iterable with for-of, is
// suitable for being bound to __iterator__ in order to make the object
// iterable in the same way via for-in.
function forInIterator() {
    for (let item of this)
        yield item;
}

exports.forInIterator = forInIterator;
