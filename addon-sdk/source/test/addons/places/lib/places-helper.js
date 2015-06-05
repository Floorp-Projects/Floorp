/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict'

const { Cc, Ci } = require('chrome');
const bmsrv = Cc['@mozilla.org/browser/nav-bookmarks-service;1'].
                    getService(Ci.nsINavBookmarksService);
const hsrv = Cc['@mozilla.org/browser/nav-history-service;1'].
              getService(Ci.nsINavHistoryService);
const brsrv = Cc["@mozilla.org/browser/nav-history-service;1"]
                     .getService(Ci.nsIBrowserHistory);
const tagsrv = Cc['@mozilla.org/browser/tagging-service;1'].
              getService(Ci.nsITaggingService);
const asyncHistory = Cc['@mozilla.org/browser/history;1'].
              getService(Ci.mozIAsyncHistory);
const { send } = require('sdk/addon/events');
const { setTimeout } = require('sdk/timers');
const { newURI } = require('sdk/url/utils');
const { defer, all } = require('sdk/core/promise');
const { once } = require('sdk/system/events');
const { set } = require('sdk/preferences/service');
const {
  Bookmark, Group, Separator,
  save, search,
  MENU, TOOLBAR, UNSORTED
} = require('sdk/places/bookmarks');

function invalidResolve (assert) {
  return function (e) {
    assert.fail('Resolve state should not be called: ' + e);
  };
}
exports.invalidResolve = invalidResolve;

function invalidReject (assert) {
  return function (e) {
    assert.fail('Reject state should not be called: ' + e);
  };
}
exports.invalidReject = invalidReject;

// Removes all children of group
function clearBookmarks (group) {
  group
   ? bmsrv.removeFolderChildren(group.id)
   : clearAllBookmarks();
}

function clearAllBookmarks () {
  [MENU, TOOLBAR, UNSORTED].forEach(clearBookmarks);
}

function clearHistory (done) {
  hsrv.removeAllPages();
  once('places-expiration-finished', done);
}

// Cleans bookmarks and history and disables maintanance
function resetPlaces (done) {
  // Set last maintenance to current time to prevent
  // Places DB maintenance occuring and locking DB
  set('places.database.lastMaintenance', Math.floor(Date.now() / 1000));
  clearAllBookmarks();
  clearHistory(done);
}
exports.resetPlaces = resetPlaces;

function compareWithHost (assert, item) {
  let id = item.id;
  let type = item.type === 'group' ? bmsrv.TYPE_FOLDER : bmsrv['TYPE_' + item.type.toUpperCase()];
  let url = item.url && !item.url.endsWith('/') ? item.url + '/' : item.url;

  if (type === bmsrv.TYPE_BOOKMARK) {
    assert.equal(url, bmsrv.getBookmarkURI(id).spec.toString(), 'Matches host url');
    let tags = tagsrv.getTagsForURI(newURI(item.url));
    for (let tag of tags) {
      // Handle both array for raw data and set for instances
      if (Array.isArray(item.tags))
        assert.ok(~item.tags.indexOf(tag), 'has correct tag');
      else
        assert.ok(item.tags.has(tag), 'has correct tag');
    }
    assert.equal(tags.length,
      Array.isArray(item.tags) ? item.tags.length : item.tags.size,
      'matches tag count');
  }
  if (type !== bmsrv.TYPE_SEPARATOR) {
    assert.equal(item.title, bmsrv.getItemTitle(id), 'Matches host title');
  }
  assert.equal(item.index, bmsrv.getItemIndex(id), 'Matches host index');
  assert.equal(item.group.id || item.group, bmsrv.getFolderIdForItem(id), 'Matches host group id');
  assert.equal(type, bmsrv.getItemType(id), 'Matches host type');
}
exports.compareWithHost = compareWithHost;

function addVisits (urls) {
  var deferred = defer();
  asyncHistory.updatePlaces([].concat(urls).map(createVisit), {
    handleResult: function () {},
    handleError: deferred.reject,
    handleCompletion: deferred.resolve
  });

  return deferred.promise;
}
exports.addVisits = addVisits;

function removeVisits (urls) {
  [].concat(urls).map(url => {
    hsrv.removePage(newURI(url));
  });
}
exports.removeVisits = removeVisits;

// Creates a mozIVisitInfo object
function createVisit (url) {
  let place = {}
  place.uri = newURI(url);
  place.title = "Test visit for " + place.uri.spec;
  place.visits = [{
    transitionType: hsrv.TRANSITION_LINK,
    visitDate: +(new Date()) * 1000,
    referredURI: undefined
  }];
  return place;
}

function createBookmark (data) {
  data = data || {};
  let item = {
    title: data.title || 'Moz',
    url: data.url || (!data.type || data.type === 'bookmark' ?
      'http://moz.com/' :
      undefined),
    tags: data.tags || (!data.type || data.type === 'bookmark' ?
      ['firefox'] :
      undefined),
    type: data.type || 'bookmark',
    group: data.group
  };
  return send('sdk-places-bookmarks-create', item);
}
exports.createBookmark = createBookmark;

function historyBatch () {
  hsrv.runInBatchMode(() => {}, null);
}
exports.historyBatch = historyBatch;

function createBookmarkItem (data) {
  let deferred = defer();
  data = data || {};
  save({
    title: data.title || 'Moz',
    url: data.url || 'http://moz.com/',
    tags: data.tags || (!data.type || data.type === 'bookmark' ?
      ['firefox'] :
      undefined),
    type: data.type || 'bookmark',
    group: data.group
  }).on('end', function (bookmark) {
    deferred.resolve(bookmark[0]);
  });
  return deferred.promise;
}
exports.createBookmarkItem = createBookmarkItem;

function createBookmarkTree () {
  let agg = [];
  return createBookmarkItem({ type: 'group', title: 'mozgroup' })
    .then(group => {
    agg.push(group);
    return all([createBookmarkItem({
      title: 'mozilla.com',
      url: 'http://mozilla.com/',
      group: group,
      tags: ['mozilla', 'firefox', 'thunderbird', 'rust']
    }), createBookmarkItem({
      title: 'mozilla.org',
      url: 'http://mozilla.org/',
      group: group,
      tags: ['mozilla', 'firefox', 'thunderbird', 'rust']
    }), createBookmarkItem({
      title: 'firefox',
      url: 'http://firefox.com/',
      group: group,
      tags: ['mozilla', 'firefox', 'browser']
    }), createBookmarkItem({
      title: 'thunderbird',
      url: 'http://mozilla.org/thunderbird/',
      group: group,
      tags: ['mozilla', 'thunderbird', 'email']
    }), createBookmarkItem({
      title: 'moz subfolder',
      group: group,
      type: 'group'
    })
    ]);
  })
  .then(results => {
    agg = agg.concat(results);
    let subfolder = results.filter(item => item.type === 'group')[0];
    return createBookmarkItem({
      title: 'dark javascript secrets',
      url: 'http://w3schools.com',
      group: subfolder,
      tags: []
    });
  }).then(item => {
    agg.push(item);
    return createBookmarkItem(
      { type: 'group', group: MENU, title: 'other stuff' }
    );
  }).then(newGroup => {
    agg.push(newGroup);
    return all([
      createBookmarkItem({
        title: 'mdn',
        url: 'http://developer.mozilla.org/en-US/',
        group: newGroup,
        tags: ['javascript']
      }),
      createBookmarkItem({
        title: 'web audio',
        url: 'http://webaud.io',
        group: newGroup,
        tags: ['javascript', 'web audio']
      }),
      createBookmarkItem({
        title: 'web audio components',
        url: 'http://component.fm',
        group: newGroup,
        tags: ['javascript', 'web audio', 'components']
      })
    ]);
  }).then(results => {
    agg = agg.concat(results);
    return agg;
  });
}
exports.createBookmarkTree = createBookmarkTree;
