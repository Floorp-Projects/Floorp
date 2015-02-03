/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require("chrome");
const memory = require('../deprecated/memory');
const { defer } = require('../core/promise');

function gc() {
  let { promise, resolve } = defer();

  Cu.forceGC();
  memory.gc();

  Cu.schedulePreciseGC(_ => resolve());

  return promise;
}
exports.gc = gc;
