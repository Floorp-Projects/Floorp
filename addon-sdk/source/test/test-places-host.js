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
const { setTimeout } = require('sdk/timers');
const { newURI } = require('sdk/url/utils');
const { send } = require('sdk/addon/events');

require('sdk/places/host/host-bookmarks');
require('sdk/places/host/host-tags');
require('sdk/places/host/host-query');
const {
  invalidResolve, invalidReject, clearBookmarks, createTree,
  compareWithHost, clearAllBookmarks, createBookmark, createBookmarkTree
} = require('./places-helper');

const bmsrv = Cc['@mozilla.org/browser/nav-bookmarks-service;1'].
                    getService(Ci.nsINavBookmarksService);
const hsrv = Cc['@mozilla.org/browser/nav-history-service;1'].
              getService(Ci.nsINavHistoryService);
const tagsrv = Cc['@mozilla.org/browser/tagging-service;1'].
              getService(Ci.nsITaggingService);
clearAllBookmarks();

exports.testBookmarksCreate = function (assert, done) {
  let items = [{
    title: 'my title',
    url: 'http://moz.com',
    tags: ['some', 'tags', 'yeah'],
    type: 'bookmark'
  }, {
    title: 'my folder',
    type: 'group',
    group: bmsrv.bookmarksMenuFolder
  }, {
    type: 'separator',
    group: bmsrv.unfiledBookmarksFolder
  }];
  
  all(items.map(function (item) {
    return send('sdk-places-bookmarks-create', item).then(function (data) {
      compareWithHost(assert, data);
    }, invalidReject(assert));
  })).then(function () {
    clearAllBookmarks();
    done();
  }, invalidReject(assert));
};

exports.testBookmarksCreateFail = function (assert, done) {
  let items = [{
    title: 'my title',
    url: 'not-a-url',
    type: 'bookmark'
  }, {
    type: 'group',
    group: bmsrv.bookmarksMenuFolder
  }, {
    group: bmsrv.unfiledBookmarksFolder
  }];
  all(items.map(function (item) {
    return send('sdk-places-bookmarks-create', item).then(null, function (reason) {
      assert.ok(reason, 'bookmark create should fail');
    });
  })).then(function () {
    clearAllBookmarks();
    done();
  });
};

exports.testBookmarkLastUpdated = function (assert, done) {
  let timestamp;
  let item;
  createBookmark().then(function (data) {
    item = data;
    timestamp = item.updated;
    return send('sdk-places-bookmarks-last-updated', { id: item.id });
  }).then(function (updated) {
    assert.equal(timestamp, updated, 'should return last updated time');
    item.title = 'updated mozilla';
    return send('sdk-places-bookmarks-save', item).then(function (data) {
      let deferred = defer();
      setTimeout(function () deferred.resolve(data), 100);
      return deferred.promise;
    });
  }).then(function (data) {
    assert.ok(data.updated > timestamp, 'time has elapsed and updated the updated property');
    clearAllBookmarks();
    done();
  });
};

exports.testBookmarkRemove = function (assert, done) {
  let id;
  createBookmark().then(function (data) {
    id = data.id;
    compareWithHost(assert, data); // ensure bookmark exists
    bmsrv.getItemTitle(id); // does not throw an error
    return send('sdk-places-bookmarks-remove', data);
  }).then(function () {
    assert.throws(function () {
      bmsrv.getItemTitle(id);
    }, 'item should no longer exist');
    clearAllBookmarks();
    done();
  }, console.error);
};

exports.testBookmarkGet = function (assert, done) {
  let bookmark;
  createBookmark().then(function (data) {
    bookmark = data;
    return send('sdk-places-bookmarks-get', { id: data.id });
  }).then(function (data) {
    'title url index group updated type tags'.split(' ').map(function (prop) {
      if (prop === 'tags') {
        for (let tag of bookmark.tags) {
          assert.ok(~data.tags.indexOf(tag),
            'correctly fetched tag ' + tag);
        }
        assert.equal(bookmark.tags.length, data.tags.length,
          'same amount of tags');
      }
      else
        assert.equal(bookmark[prop], data[prop], 'correctly fetched ' + prop);
    });
    clearAllBookmarks();
    done();
  });
};

exports.testTagsTag = function (assert, done) {
  let url;
  createBookmark().then(function (data) {
    url = data.url;
    return send('sdk-places-tags-tag', {
      url: data.url, tags: ['mozzerella', 'foxfire']
    });
  }).then(function () {
    let tags = tagsrv.getTagsForURI(newURI(url));
    assert.ok(~tags.indexOf('mozzerella'), 'first tag found');
    assert.ok(~tags.indexOf('foxfire'), 'second tag found');
    assert.ok(~tags.indexOf('firefox'), 'default tag found');
    assert.equal(tags.length, 3, 'no extra tags');
    clearAllBookmarks();
    done();
  });
};

exports.testTagsUntag = function (assert, done) {
  let item;
  createBookmark({tags: ['tag1', 'tag2', 'tag3']}).then(function (data) {
    item = data;
    return send('sdk-places-tags-untag', {
      url: item.url,
      tags: ['tag2', 'firefox']
    });
  }).then(function () {
    let tags = tagsrv.getTagsForURI(newURI(item.url));
    assert.ok(~tags.indexOf('tag1'), 'first tag persisted');
    assert.ok(~tags.indexOf('tag3'), 'second tag persisted');
    assert.ok(!~tags.indexOf('firefox'), 'first tag removed');
    assert.ok(!~tags.indexOf('tag2'), 'second tag removed');
    assert.equal(tags.length, 2, 'no extra tags');
    clearAllBookmarks();
    done();
  });
};

exports.testTagsGetURLsByTag = function (assert, done) {
  let item;
  createBookmark().then(function (data) {
    item = data;
    return send('sdk-places-tags-get-urls-by-tag', {
      tag: 'firefox'
    });
  }).then(function(urls) {
    assert.equal(item.url, urls[0], 'returned correct url');
    assert.equal(urls.length, 1, 'returned only one url');
    clearAllBookmarks();
    done();
  });
};

exports.testTagsGetTagsByURL = function (assert, done) {
  let item;
  createBookmark({ tags: ['firefox', 'mozilla', 'metal']}).then(function (data) {
    item = data;
    return send('sdk-places-tags-get-tags-by-url', {
      url: data.url,
    });
  }).then(function(tags) {
    assert.ok(~tags.indexOf('firefox'), 'returned first tag');
    assert.ok(~tags.indexOf('mozilla'), 'returned second tag');
    assert.ok(~tags.indexOf('metal'), 'returned third tag');
    assert.equal(tags.length, 3, 'returned all tags');
    clearAllBookmarks();
    done();
  });
};

exports.testHostQuery = function (assert, done) {
  all([
    createBookmark({ url: 'http://firefox.com', tags: ['firefox', 'mozilla'] }),
    createBookmark({ url: 'http://mozilla.com', tags: ['mozilla'] }),
    createBookmark({ url: 'http://thunderbird.com' })
  ]).then(data => {
    return send('sdk-places-query', {
      queries: { tags: ['mozilla'] }, 
      options: { sortingMode: 6, queryType: 1 } // sort by URI ascending, bookmarks only
    });
  }).then(results => {
    assert.equal(results.length, 2, 'should only return two');
    assert.equal(results[0].url, 'http://mozilla.com/', 'is sorted by URI asc');
    return send('sdk-places-query', {
      queries: { tags: ['mozilla'] }, 
      options: { sortingMode: 5, queryType: 1 } // sort by URI descending, bookmarks only
    });
  }).then(results => {
    assert.equal(results.length, 2, 'should only return two');
    assert.equal(results[0].url, 'http://firefox.com/', 'is sorted by URI desc');
    clearAllBookmarks();
    done();
  });
};

exports.testHostMultiQuery = function (assert, done) {
  all([
    createBookmark({ url: 'http://firefox.com', tags: ['firefox', 'mozilla'] }),
    createBookmark({ url: 'http://mozilla.com', tags: ['mozilla'] }),
    createBookmark({ url: 'http://thunderbird.com' })
  ]).then(data => {
    return send('sdk-places-query', {
      queries: [{ tags: ['firefox'] }, { uri: 'http://thunderbird.com/' }],
      options: { sortingMode: 5, queryType: 1 } // sort by URI descending, bookmarks only
    });
  }).then(results => {
    assert.equal(results.length, 2, 'should return 2 results ORing queries');
    assert.equal(results[0].url, 'http://firefox.com/', 'should match URL or tag');
    assert.equal(results[1].url, 'http://thunderbird.com/', 'should match URL or tag');
    return send('sdk-places-query', {
      queries: [{ tags: ['firefox'], url: 'http://mozilla.com/' }],
      options: { sortingMode: 5, queryType: 1 } // sort by URI descending, bookmarks only
    });
  }).then(results => {
    assert.equal(results.length, 0, 'query props should be AND\'d');
    clearAllBookmarks();
    done();
  });
};

exports.testGetAllBookmarks = function (assert, done) {
  createBookmarkTree().then(() => { 
    return send('sdk-places-bookmarks-get-all', {});
  }).then(res => {
    assert.equal(res.length, 8, 'all bookmarks returned');
    clearAllBookmarks();
    done();
  }, console.error);
};

exports.testGetAllChildren = function (assert, done) {
  createBookmarkTree().then(results => {
    return send('sdk-places-bookmarks-get-children', {
      id: results.filter(({title}) => title === 'mozgroup')[0].id
    });
  }).then(results => {
    assert.equal(results.length, 5,
      'should return all children and folders at a single depth');
    clearAllBookmarks();
    done();
  });
};

require('test').run(exports);
