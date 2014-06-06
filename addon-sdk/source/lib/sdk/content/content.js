/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const { deprecateUsage } = require('../util/deprecate');

Object.defineProperty(exports, "Loader", { 
  get: function() {
    deprecateUsage('`sdk/content/content` is deprecated. Please use `sdk/content/loader` directly.');
    return require('./loader').Loader;
  }
});

Object.defineProperty(exports, "Symbiont", { 
  get: function() {
    deprecateUsage('Both `sdk/content/content` and `sdk/deprecated/symbiont` are deprecated. ' +
                   '`sdk/core/heritage` supersedes Symbiont for inheritance.');
    return require('../deprecated/symbiont').Symbiont;
  }
});

Object.defineProperty(exports, "Worker", { 
  get: function() {
    deprecateUsage('`sdk/content/content` is deprecated. Please use `sdk/content/worker` directly.');
    return require('./worker').Worker;
  }
});
