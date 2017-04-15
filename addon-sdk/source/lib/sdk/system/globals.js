/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

defineLazyGetter(this, "console", () => new (require('../console/plain-text').PlainTextConsole)());

exports = module.exports = {};

defineLazyGetter(exports, "console", () => console);

// Provide CommonJS `define` to allow authoring modules in a format that can be
// loaded both into jetpack and into browser via AMD loaders.

// `define` is provided as a lazy getter that binds below defined `define`
// function to the module scope, so that require, exports and module
// variables remain accessible.
defineLazyGetter(exports, 'define', function() {
  return (...args) => {
    let factory = args.pop();
    factory.call(this, this.require, this.exports, this.module);
  };
});

function defineLazyGetter(object, prop, getter) {
  let redefine = (obj, value) => {
    Object.defineProperty(obj, prop, {
      configurable: true,
      writable: true,
      value,
    });
    return value;
  };

  Object.defineProperty(object, prop, {
    configurable: true,
    get() {
      return redefine(this, getter.call(this));
    },
    set(value) {
      redefine(this, value);
    }
  });
}
