/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "*",
    "SeaMonkey": "*"
  }
};

const { Cc, Ci } = require('chrome');
const { defer, all, resolve } = require('../../core/promise');
const { safeMerge, omit } = require('../../util/object');
const historyService = Cc['@mozilla.org/browser/nav-history-service;1']
                     .getService(Ci.nsINavHistoryService);
const bookmarksService = Cc['@mozilla.org/browser/nav-bookmarks-service;1']
                         .getService(Ci.nsINavBookmarksService);
const { request, response } = require('../../addon/host');
const { newURI } = require('../../url/utils');
const { send } = require('../../addon/events');
const { on, emit } = require('../../event/core');
const { filter } = require('../../event/utils');

const ROOT_FOLDERS = [
  bookmarksService.unfiledBookmarksFolder, bookmarksService.toolbarFolder,
  bookmarksService.bookmarksMenuFolder
];

const EVENT_MAP = {
  'sdk-places-query': queryReceiver
};

// Properties that need to be manually
// copied into a nsINavHistoryQuery object
const MANUAL_QUERY_PROPERTIES = [
  'uri', 'folder', 'tags', 'url', 'folder'
];

const PLACES_PROPERTIES = [
  'uri', 'title', 'accessCount', 'time'
];

function execute (queries, options) {
  let deferred = defer();
  let root = historyService
    .executeQueries(queries, queries.length, options).root;

  let items = collect([], root);
  deferred.resolve(items);
  return deferred.promise;
}

function collect (acc, node) {
  node.containerOpen = true;
  for (let i = 0; i < node.childCount; i++) {
    let child = node.getChild(i);
    acc.push(child);
    if (child.type === child.RESULT_TYPE_FOLDER) {
      let container = child.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      collect(acc, container);
    }
  }
  node.containerOpen = false;
  return acc;
}

function query (queries, options) {
  queries = queries || [];
  options = options || {}; 
  let deferred = defer();
  let optionsObj, queryObjs;

  try {
    optionsObj = historyService.getNewQueryOptions();
    queryObjs = [].concat(queries).map(createQuery);
    if (!queryObjs.length) {
      queryObjs = [historyService.getNewQuery()];
    }
    safeMerge(optionsObj, options);
  } catch (e) {
    deferred.reject(e);
    return deferred.promise;
  }

  /*
   * Currently `places:` queries are not supported
   */
  optionsObj.excludeQueries = true;

  execute(queryObjs, optionsObj).then(function (results) {
    if (optionsObj.queryType === 0) {
      return results.map(normalize);
    } else if (optionsObj.queryType === 1) {
      // Formats query results into more standard
      // data structures for returning
      return all(results.map(({itemId}) =>
        send('sdk-places-bookmarks-get', { id: itemId })));
    }
  }).then(deferred.resolve, deferred.reject);
  
  return deferred.promise;
}
exports.query = query;

function createQuery (query) {
  query = query || {};
  let queryObj = historyService.getNewQuery();

  safeMerge(queryObj, omit(query, MANUAL_QUERY_PROPERTIES));

  if (query.tags && Array.isArray(query.tags))
    queryObj.tags = query.tags;
  if (query.uri || query.url)
    queryObj.uri = newURI(query.uri || query.url);
  if (query.folder)
    queryObj.setFolders([query.folder], 1);
  return queryObj;
}

function queryReceiver (message) {
  let queries = message.data.queries || message.data.query;
  let options = message.data.options;
  let resData = {
    id: message.id,
    event: message.event
  };

  query(queries, options).then(results => {
    resData.data = results;
    respond(resData);
  }, reason => {
    resData.error = reason;
    respond(resData);
  });
}

/*
 * Converts a nsINavHistoryResultNode into a plain object
 * 
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryResultNode
 */
function normalize (historyObj) {
  return PLACES_PROPERTIES.reduce((obj, prop) => {
    if (prop === 'uri')
      obj.url = historyObj.uri;
    else if (prop === 'time') {
      // Cast from microseconds to milliseconds
      obj.time = Math.floor(historyObj.time / 1000)
    } else if (prop === 'accessCount')
      obj.visitCount = historyObj[prop];
    else
      obj[prop] = historyObj[prop];
    return obj;
  }, {});
}

/*
 * Hook into host
 */

let reqStream = filter(request, function (data) /sdk-places-query/.test(data.event));
on(reqStream, 'data', function (e) {
  if (EVENT_MAP[e.event]) EVENT_MAP[e.event](e);
});

function respond (data) {
  emit(response, 'data', data);
}
