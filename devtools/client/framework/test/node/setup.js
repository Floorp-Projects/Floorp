/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global global */

global.loader = {
  lazyGetter: (context, name, fn) => {
    const module = fn();
    global[name] = module;
  },
  lazyRequireGetter: (context, name, module, destructure) => {
    const value = destructure ? require(module)[name] : require(module || name);
    global[name] = value;
  },
};

global.define = function(fn) {
  fn(null, global, { exports: global });
};

global.requestIdleCallback = function() {};
global.isWorker = false;
