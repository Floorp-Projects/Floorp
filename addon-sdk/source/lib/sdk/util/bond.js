/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const makeDescriptor = (name, method) => ({
  get() {
    if (!Object.hasOwnProperty.call(this, name)) {
      Object.defineProperty(this, name, {value: method.bind(this)});
      return this[name];
    } else {
      return method;
    }
  }
});

const Bond = function(methods) {
  let descriptor = {};
  let members = [...Object.getOwnPropertyNames(methods),
                 ...Object.getOwnPropertySymbols(methods)];

  for (let name of members) {
    let method = methods[name];
    if (typeof(method) !== "function") {
      throw new TypeError(`Property named "${name}" passed to Bond must be a function`);
    }
    descriptor[name] = makeDescriptor(name, method);
  }

  return Object.create(Bond.prototype, descriptor);
}
exports.Bond = Bond;
