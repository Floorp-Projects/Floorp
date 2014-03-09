/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cc, Ci } = require('chrome');
const { defer, all } = require('sdk/core/promise');
const { filter } = require('sdk/event/utils');
const { on, off } = require('sdk/event/core');
const { events } = require('sdk/places/events');
const { setTimeout } = require('sdk/timers');
const { before, after } = require('sdk/test/utils');
const bmsrv = Cc['@mozilla.org/browser/nav-bookmarks-service;1'].
                getService(Ci.nsINavBookmarksService);
const {
  search
} = require('sdk/places/history');
const {
  invalidResolve, invalidReject, createTree, createBookmark,
  compareWithHost, addVisits, resetPlaces, createBookmarkItem,
  removeVisits
} = require('../places-helper');
const { save, MENU, UNSORTED } = require('sdk/places/bookmarks');
const { promisedEmitter } = require('sdk/places/utils');

exports['test bookmark-item-added'] = function (assert, done) {
  function handler ({type, data}) {
    if (type !== 'bookmark-item-added') return;
    if (data.title !== 'bookmark-added-title') return;

    assert.equal(type, 'bookmark-item-added', 'correct type in bookmark-added event');
    assert.equal(data.type, 'bookmark', 'correct data in bookmark-added event');
    assert.ok(data.id != null, 'correct data in bookmark-added event');
    assert.ok(data.parentId != null, 'correct data in bookmark-added event');
    assert.ok(data.index != null, 'correct data in bookmark-added event');
    assert.equal(data.url, 'http://moz.com/', 'correct data in bookmark-added event');
    assert.ok(data.dateAdded != null, 'correct data in bookmark-added event');
    events.off('data', handler);
    done();
  }
  events.on('data', handler);
  createBookmark({ title: 'bookmark-added-title' });
};

exports['test bookmark-item-changed'] = function (assert, done) {
  let id;
  let complete = makeCompleted(done);
  function handler ({type, data}) {
    if (type !== 'bookmark-item-changed') return;
    if (data.id !== id) return;
    // Abort if the 'bookmark-item-changed' event isn't for the `title` property,
    // as sometimes the event can be for the `url` property.
    // Intermittent failure, bug 969616
    if (data.property !== 'title') return;

    assert.equal(type, 'bookmark-item-changed',
      'correct type in bookmark-item-changed event');
    assert.equal(data.type, 'bookmark',
      'correct data in bookmark-item-changed event');
    assert.equal(data.property, 'title',
      'correct property in bookmark-item-changed event');
    assert.equal(data.value, 'bookmark-changed-title-2',
      'correct value in bookmark-item-changed event');
    assert.ok(data.id === id, 'correct id in bookmark-item-changed event');
    assert.ok(data.parentId != null, 'correct data in bookmark-added event');

    events.off('data', handler);
    complete();
  }
  events.on('data', handler);

  createBookmarkItem({ title: 'bookmark-changed-title' }).then(item => {
    id = item.id;
    item.title = 'bookmark-changed-title-2';
    return saveP(item);
  }).then(complete);
};

exports['test bookmark-item-moved'] = function (assert, done) {
  let id;
  let complete = makeCompleted(done);
  let previousIndex, previousParentId;

  function handler ({type, data}) {
    if (type !== 'bookmark-item-moved') return;
    if (data.id !== id) return;
    assert.equal(type, 'bookmark-item-moved',
      'correct type in bookmark-item-moved event');
    assert.equal(data.type, 'bookmark',
      'correct data in bookmark-item-moved event');
    assert.ok(data.id === id, 'correct id in bookmark-item-moved event');
    assert.equal(data.previousParentId, previousParentId,
      'correct previousParentId');
    assert.equal(data.currentParentId, bmsrv.getFolderIdForItem(id),
      'correct currentParentId');
    assert.equal(data.previousIndex, previousIndex, 'correct previousIndex');
    assert.equal(data.currentIndex, bmsrv.getItemIndex(id), 'correct currentIndex');

    events.off('data', handler);
    complete();
  }
  events.on('data', handler);

  createBookmarkItem({
    title: 'bookmark-moved-title',
    group: UNSORTED
  }).then(item => {
    id = item.id;
    previousIndex = bmsrv.getItemIndex(id);
    previousParentId = bmsrv.getFolderIdForItem(id);
    item.group = MENU;
    return saveP(item);
  }).then(complete);
};

exports['test bookmark-item-removed'] = function (assert, done) {
  let id;
  let complete = makeCompleted(done);
  function handler ({type, data}) {
    if (type !== 'bookmark-item-removed') return;
    if (data.id !== id) return;
    assert.equal(type, 'bookmark-item-removed',
      'correct type in bookmark-item-removed event');
    assert.equal(data.type, 'bookmark',
      'correct data in bookmark-item-removed event');
    assert.ok(data.id === id, 'correct id in bookmark-item-removed event');
    assert.equal(data.parentId, UNSORTED.id,
      'correct parentId in bookmark-item-removed');
    assert.equal(data.url, 'http://moz.com/',
      'correct url in bookmark-item-removed event');
    assert.equal(data.index, 0,
      'correct index in bookmark-item-removed event');

    events.off('data', handler);
    complete();
  }
  events.on('data', handler);

  createBookmarkItem({
    title: 'bookmark-item-remove-title',
    group: UNSORTED
  }).then(item => {
    id = item.id;
    item.remove = true;
    return saveP(item);
  }).then(complete);
};

exports['test bookmark-item-visited'] = function (assert, done) {
  let id;
  let complete = makeCompleted(done);
  function handler ({type, data}) {
    if (type !== 'bookmark-item-visited') return;
    if (data.id !== id) return;
    assert.equal(type, 'bookmark-item-visited',
      'correct type in bookmark-item-visited event');
    assert.ok(data.id === id, 'correct id in bookmark-item-visited event');
    assert.equal(data.parentId, UNSORTED.id,
      'correct parentId in bookmark-item-visited');
    assert.ok(data.transitionType != null,
      'has a transition type in bookmark-item-visited event');
    assert.ok(data.time != null,
      'has a time in bookmark-item-visited event');
    assert.ok(data.visitId != null,
      'has a visitId in bookmark-item-visited event');
    assert.equal(data.url, 'http://bookmark-item-visited.com/',
      'correct url in bookmark-item-visited event');

    events.off('data', handler);
    complete();
  }
  events.on('data', handler);

  createBookmarkItem({
    title: 'bookmark-item-visited',
    url: 'http://bookmark-item-visited.com/'
  }).then(item => {
    id = item.id;
    return addVisits('http://bookmark-item-visited.com/');
  }).then(complete);
};

exports['test history-start-batch, history-end-batch, history-start-clear'] = function (assert, done) {
  let complete = makeCompleted(done, 4);
  let startEvent = filter(events, ({type}) => type === 'history-start-batch');
  let endEvent = filter(events, ({type}) => type === 'history-end-batch');
  let clearEvent = filter(events, ({type}) => type === 'history-start-clear');
  function startHandler ({type, data}) {
    assert.pass('history-start-batch called');
    assert.equal(type, 'history-start-batch',
      'history-start-batch has correct type');
    off(startEvent, 'data', startHandler);
    on(endEvent, 'data', endHandler);
    complete();
  }
  function endHandler ({type, data}) {
    assert.pass('history-end-batch called');
    assert.equal(type, 'history-end-batch',
      'history-end-batch has correct type');
    off(endEvent, 'data', endHandler);
    complete();
  }
  function clearHandler ({type, data}) {
    assert.pass('history-start-clear called');
    assert.equal(type, 'history-start-clear',
      'history-start-clear has correct type');
    off(clearEvent, 'data', clearHandler);
    complete();
  }
 
  on(startEvent, 'data', startHandler);
  on(clearEvent, 'data', clearHandler);

  createBookmark().then(() => {
    resetPlaces(complete);
  })
};

exports['test history-visit, history-title-changed'] = function (assert, done) {
  let complete = makeCompleted(() => {
    off(titleEvents, 'data', titleHandler);
    off(visitEvents, 'data', visitHandler);
    done();
  }, 6);
  let visitEvents = filter(events, ({type}) => type === 'history-visit');
  let titleEvents = filter(events, ({type}) => type === 'history-title-changed');

  let urls = ['http://moz.com/', 'http://firefox.com/', 'http://mdn.com/'];

  function visitHandler ({type, data}) {
    assert.equal(type, 'history-visit', 'correct type in history-visit');
    assert.ok(~urls.indexOf(data.url), 'history-visit has correct url');
    assert.ok(data.visitId != null, 'history-visit has a visitId');
    assert.ok(data.time != null, 'history-visit has a time');
    assert.ok(data.sessionId != null, 'history-visit has a sessionId');
    assert.ok(data.referringId != null, 'history-visit has a referringId');
    assert.ok(data.transitionType != null, 'history-visit has a transitionType');
    complete();
  }

  function titleHandler ({type, data}) {
    assert.equal(type, 'history-title-changed',
      'correct type in history-title-changed');
    assert.ok(~urls.indexOf(data.url),
      'history-title-changed has correct url');
    assert.ok(data.title, 'history-title-changed has title');
    complete();
  }

  on(titleEvents, 'data', titleHandler);
  on(visitEvents, 'data', visitHandler);
  addVisits(urls);
}

exports['test history-delete-url'] = function (assert, done) {
  let complete = makeCompleted(() => {
    events.off('data', handler);
    done();
  }, 3);
  let urls = ['http://moz.com/', 'http://firefox.com/', 'http://mdn.com/'];
  function handler({type, data}) {
    if (type !== 'history-delete-url') return;
    assert.equal(type, 'history-delete-url',
      'history-delete-url has correct type');
    assert.ok(~urls.indexOf(data.url), 'history-delete-url has correct url');
    complete();
  }

  events.on('data', handler);
  addVisits(urls).then(() => {
    removeVisits(urls);
  });
};

exports['test history-page-changed'] = function (assert) {
  assert.pass('history-page-changed tested in test-places-favicons');
};

exports['test history-delete-visits'] = function (assert) {
  assert.pass('TODO test history-delete-visits');
};

before(exports, (name, assert, done) => resetPlaces(done));
after(exports, (name, assert, done) => resetPlaces(done));

function saveP () {
  return promisedEmitter(save.apply(null, Array.slice(arguments)));
}

function makeCompleted (done, countTo) {
  let count = 0;
  countTo = countTo || 2;
  return function () {
    if (++count === countTo) done();
  };
}
