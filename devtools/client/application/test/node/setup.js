/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global global */

// Configure enzyme with React 16 adapter.
const Enzyme = require("enzyme");
const Adapter = require("enzyme-adapter-react-16");
Enzyme.configure({ adapter: new Adapter() });

global.loader = {
  lazyGetter: (context, name, fn) => {
    const module = fn();
    global[name] = module;
  },
  lazyRequireGetter: (obj, property, module, destructure) => {
    Object.defineProperty(obj, property, {
      get: () => {
        // Redefine this accessor property as a data property.
        // Delete it first, to rule out "too much recursion" in case obj is
        // a proxy whose defineProperty handler might unwittingly trigger this
        // getter again.
        delete obj[property];
        const value = destructure
          ? require(module)[property]
          : require(module || property);
        Object.defineProperty(obj, property, {
          value,
          writable: true,
          configurable: true,
          enumerable: true,
        });
        return value;
      },
      configurable: true,
      enumerable: true,
    });
  },
  lazyImporter: () => {},
};
