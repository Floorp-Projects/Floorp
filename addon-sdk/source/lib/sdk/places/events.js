/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '*',
    "SeaMonkey": '*'
  }
};

const { Cc, Ci } = require('chrome');
const { Unknown } = require('../platform/xpcom');
const { Class } = require('../core/heritage');
const { merge } = require('../util/object');
const bookmarkService = Cc['@mozilla.org/browser/nav-bookmarks-service;1']
                        .getService(Ci.nsINavBookmarksService);
const historyService = Cc['@mozilla.org/browser/nav-history-service;1']
                       .getService(Ci.nsINavHistoryService);
const { mapBookmarkItemType } = require('./utils');
const { EventTarget } = require('../event/target');
const { emit } = require('../event/core');
const { when } = require('../system/unload');

const emitter = EventTarget();

var HISTORY_ARGS = {
  onBeginUpdateBatch: [],
  onEndUpdateBatch: [],
  onClearHistory: [],
  onDeleteURI: ['url'],
  onDeleteVisits: ['url', 'visitTime'],
  onPageChanged: ['url', 'property', 'value'],
  onTitleChanged: ['url', 'title'],
  onVisit: [
    'url', 'visitId', 'time', 'sessionId', 'referringId', 'transitionType'
  ]
};

var HISTORY_EVENTS = {
  onBeginUpdateBatch: 'history-start-batch',
  onEndUpdateBatch: 'history-end-batch',
  onClearHistory: 'history-start-clear',
  onDeleteURI: 'history-delete-url',
  onDeleteVisits: 'history-delete-visits',
  onPageChanged: 'history-page-changed',
  onTitleChanged: 'history-title-changed',
  onVisit: 'history-visit'
};

var BOOKMARK_ARGS = {
  onItemAdded: [
    'id', 'parentId', 'index', 'type', 'url', 'title', 'dateAdded'
  ],
  onItemChanged: [
    'id', 'property', null, 'value', 'lastModified', 'type', 'parentId'
  ],
  onItemMoved: [
    'id', 'previousParentId', 'previousIndex', 'currentParentId',
    'currentIndex', 'type'
  ],
  onItemRemoved: ['id', 'parentId', 'index', 'type', 'url'],
  onItemVisited: ['id', 'visitId', 'time', 'transitionType', 'url', 'parentId']
};

var BOOKMARK_EVENTS = {
  onItemAdded: 'bookmark-item-added',
  onItemChanged: 'bookmark-item-changed',
  onItemMoved: 'bookmark-item-moved',
  onItemRemoved: 'bookmark-item-removed',
  onItemVisited: 'bookmark-item-visited',
};

function createHandler (type, propNames) {
  propNames = propNames || [];
  return function (...args) {
    let data = propNames.reduce((acc, prop, i) => {
      if (prop)
        acc[prop] = formatValue(prop, args[i]);
      return acc;
    }, {});

    emit(emitter, 'data', {
      type: type,
      data: data
    });
  };
}

/*
 * Creates an observer, creating handlers based off of
 * the `events` names, and ordering arguments from `propNames` hash
 */
function createObserverInstance (events, propNames) {
  let definition = Object.keys(events).reduce((prototype, eventName) => {
    prototype[eventName] = createHandler(events[eventName], propNames[eventName]);
    return prototype;
  }, {});

  return Class(merge(definition, { extends: Unknown }))();
}

/*
 * Formats `data` based off of the value of `type`
 */
function formatValue (type, data) {
  if (type === 'type')
    return mapBookmarkItemType(data);
  if (type === 'url' && data)
    return data.spec;
  return data;
}

var historyObserver = createObserverInstance(HISTORY_EVENTS, HISTORY_ARGS);
historyService.addObserver(historyObserver, false);

var bookmarkObserver = createObserverInstance(BOOKMARK_EVENTS, BOOKMARK_ARGS);
bookmarkService.addObserver(bookmarkObserver, false);

when(() => {
  historyService.removeObserver(historyObserver);
  bookmarkService.removeObserver(bookmarkObserver);
});

exports.events = emitter;
