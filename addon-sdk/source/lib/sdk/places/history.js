/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable",
  "engines": {
    "Firefox": "*"
  }
};

/*
 * Requiring hosts so they can subscribe to client messages
 */
require('./host/host-bookmarks');
require('./host/host-tags');
require('./host/host-query');

const { Cc, Ci } = require('chrome');
const { Class } = require('../core/heritage');
const { events, send } = require('../addon/events');
const { defer, reject, all } = require('../core/promise');
const { uuid } = require('../util/uuid');
const { flatten } = require('../util/array');
const { has, extend, merge, pick } = require('../util/object');
const { emit } = require('../event/core');
const { defer: async } = require('../lang/functional');
const { EventTarget } = require('../event/target');
const {
  urlQueryParser, createQuery, createQueryOptions
} = require('./utils');

/*
 * Constant used by nsIHistoryQuery; 0 is a history query
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryQueryOptions
 */
const HISTORY_QUERY = 0;

let search = function query (queries, options) {
  queries = [].concat(queries);
  let emitter = EventTarget();
  let queryObjs = queries.map(createQuery.bind(null, HISTORY_QUERY));
  let optionsObj = createQueryOptions(HISTORY_QUERY, options);

  // Can remove after `Promise.jsm` is implemented in Bug 881047,
  // which will guarantee next tick execution
  async(() => {
    send('sdk-places-query', {
      query: queryObjs,
      options: optionsObj
    }).then(results => {
      results.map(item => emit(emitter, 'data', item));
      emit(emitter, 'end', results);
    }, reason => {
      emit(emitter, 'error', reason);
      emit(emitter, 'end', []);
    });
  })();

  return emitter;
};
exports.search = search;
