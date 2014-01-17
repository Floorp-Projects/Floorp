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
