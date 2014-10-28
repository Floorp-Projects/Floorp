/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict'

const { Cc, Ci, Cu } = require('chrome');
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

function invalidResolve (assert) {
  return function (e) {
    assert.fail('Resolve state should not be called: ' + e);
  };
}
exports.invalidResolve = invalidResolve;

// Removes all children of group
function clearBookmarks (group) {
  group
   ? PlacesUtils.bookmarks.removeFolderChildren(group.id)
   : clearAllBookmarks();
}

function clearAllBookmarks () {
  [MENU, TOOLBAR, UNSORTED].forEach(clearBookmarks);
}

function clearHistory (done) {
  PlacesUtils.history.clear().catch(Cu.reportError).then(done);
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
  let type = item.type === 'group' ?
    PlacesUtils.bookmarks.TYPE_FOLDER :
    PlacesUtils.bookmarks['TYPE_' + item.type.toUpperCase()];
  let url = item.url && !item.url.endsWith('/') ? item.url + '/' : item.url;

  if (type === PlacesUtils.bookmarks.TYPE_BOOKMARK) {
    assert.equal(url, PlacesUtils.bookmarks.getBookmarkURI(id).spec.toString(),
                 'Matches host url');
    let tags = PlacesUtils.tagging.getTagsForURI(newURI(item.url));
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
  if (type !== PlacesUtils.bookmarks.TYPE_SEPARATOR) {
    assert.equal(item.title, PlacesUtils.bookmarks.getItemTitle(id),
                 'Matches host title');
  }
  assert.equal(item.index, PlacesUtils.bookmarks.getItemIndex(id),
               'Matches host index');
  assert.equal(item.group.id || item.group,
               PlacesUtils.bookmarks.getFolderIdForItem(id),
               'Matches host group id');
  assert.equal(type, PlacesUtils.bookmarks.getItemType(id),
               'Matches host type');
}
exports.compareWithHost = compareWithHost;

/**
 * Adds visits to places.
 *
 * @param {Array|String} urls Either an array of urls to add, or a single url
 *                            as a string.
 */
function addVisits (urls) {
  return PlacesUtils.history.insertMany([].concat(urls).map(createVisit));
}
exports.addVisits = addVisits;

function removeVisits (urls) {
  PlacesUtils.history.remove(urls);
}
exports.removeVisits = removeVisits;

// Creates a mozIVisitInfo object
function createVisit (url) {
  return {
    url,
    title: "Test visit for " + url,
    visits: [{
      transition: PlacesUtils.history.TRANSITION_LINK
    }]
  };
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
  PlacesUtils.history.runInBatchMode(() => {}, null);
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
