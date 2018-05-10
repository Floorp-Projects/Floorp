"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let newSources;
let createSource;
let supportsWasm = false;
let queuedSources;
let currentWork;

async function dispatchNewSources() {
  const sources = queuedSources;
  queuedSources = [];
  currentWork = await newSources(sources.map(source => createSource(source, {
    supportsWasm
  })));
}

const queue = (0, _lodash.throttle)(dispatchNewSources, 100);
exports.default = {
  initialize: options => {
    newSources = options.actions.newSources;
    createSource = options.createSource;
    supportsWasm = options.supportsWasm;
    queuedSources = [];
  },
  queue: source => {
    queuedSources.push(source);
    queue();
  },
  flush: () => Promise.all([queue.flush(), currentWork]),
  clear: () => queue.cancel()
};