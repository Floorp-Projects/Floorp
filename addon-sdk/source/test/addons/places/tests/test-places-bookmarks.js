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
const { request } = require('sdk/addon/host');
const { filter } = require('sdk/event/utils');
const { on, off } = require('sdk/event/core');
const { setTimeout } = require('sdk/timers');
const { newURI } = require('sdk/url/utils');
const { defer, all } = require('sdk/core/promise');
const { defer: async } = require('sdk/lang/functional');
const { before, after } = require('sdk/test/utils');

const {
  Bookmark, Group, Separator,
  save, search, remove,
  MENU, TOOLBAR, UNSORTED
} = require('sdk/places/bookmarks');
const {
  invalidResolve, invalidReject, createTree,
  compareWithHost, createBookmark, createBookmarkItem,
  createBookmarkTree, addVisits, resetPlaces
} = require('../places-helper');
const { promisedEmitter } = require('sdk/places/utils');
const bmsrv = Cc['@mozilla.org/browser/nav-bookmarks-service;1'].
                    getService(Ci.nsINavBookmarksService);
const tagsrv = Cc['@mozilla.org/browser/tagging-service;1'].
                    getService(Ci.nsITaggingService);

exports.testDefaultFolders = function (assert) {
  var ids = [
    bmsrv.bookmarksMenuFolder,
    bmsrv.toolbarFolder,
    bmsrv.unfiledBookmarksFolder
  ];
  [MENU, TOOLBAR, UNSORTED].forEach(function (g, i) {
    assert.ok(g.id === ids[i], ' default group matches id');
  });
};

exports.testValidation = function (assert) {
  assert.throws(() => {
    Bookmark({ title: 'a title' });
  }, /The `url` property must be a valid URL/, 'throws empty URL error');

  assert.throws(() => {
    Bookmark({ title: 'a title', url: 'not.a.url' });
  }, /The `url` property must be a valid URL/, 'throws invalid URL error');

  assert.throws(() => {
    Bookmark({ url: 'http://foo.com' });
  }, /The `title` property must be defined/, 'throws title error');

  assert.throws(() => {
    Bookmark();
  }, /./, 'throws any error');

  assert.throws(() => {
    Group();
  }, /The `title` property must be defined/, 'throws title error for group');

  assert.throws(() => {
    Bookmark({ url: 'http://foo.com', title: 'my title', tags: 'a tag' });
  }, /The `tags` property must be a Set, or an array/, 'throws error for non set/array tag');
};

exports.testCreateBookmarks = function (assert, done) {
  var bm = Bookmark({
    title: 'moz',
    url: 'http://mozilla.org',
    tags: ['moz1', 'moz2', 'moz3']
  });

  save(bm).on('data', (bookmark, input) => {
    assert.equal(input, bm, 'input is original input item');
    assert.ok(bookmark.id, 'Bookmark has ID');
    assert.equal(bookmark.title, 'moz');
    assert.equal(bookmark.url, 'http://mozilla.org');
    assert.equal(bookmark.group, UNSORTED, 'Unsorted folder is default parent');
    assert.ok(bookmark !== bm, 'bookmark should be a new instance');
    compareWithHost(assert, bookmark);
  }).on('end', bookmarks => {
    assert.equal(bookmarks.length, 1, 'returned bookmarks in end');
    assert.equal(bookmarks[0].url, 'http://mozilla.org');
    assert.equal(bookmarks[0].tags.has('moz1'), true, 'has first tag');
    assert.equal(bookmarks[0].tags.has('moz2'), true, 'has second tag');
    assert.equal(bookmarks[0].tags.has('moz3'), true, 'has third tag');
    assert.pass('end event is called');
    done();
  });
};

exports.testCreateGroup = function (assert, done) {
  save(Group({ title: 'mygroup', group: MENU })).on('data', g => {
    assert.ok(g.id, 'Bookmark has ID');
    assert.equal(g.title, 'mygroup', 'matches title');
    assert.equal(g.group, MENU, 'Menu folder matches');
    compareWithHost(assert, g);
  }).on('end', results => {
    assert.equal(results.length, 1);
    assert.pass('end event is called');
    done();
  });
};

exports.testCreateSeparator = function (assert, done) {
  save(Separator({ group: MENU })).on('data', function (s) {
    assert.ok(s.id, 'Separator has id');
    assert.equal(s.group, MENU, 'Parent group matches');
    compareWithHost(assert, s);
  }).on('end', function (results) {
    assert.equal(results.length, 1);
    assert.pass('end event is called');
    done();
  });
};

exports.testCreateError = function (assert, done) {
  let bookmarks = [
    { title: 'moz1', url: 'http://moz1.com', type: 'bookmark'},
    { title: 'moz2', url: 'invalidurl', type: 'bookmark'},
    { title: 'moz3', url: 'http://moz3.com', type: 'bookmark'}
  ];

  let dataCount = 0, errorCount = 0;
  save(bookmarks).on('data', bookmark => {
    assert.ok(/moz[1|3]/.test(bookmark.title), 'valid bookmarks complete');
    dataCount++;
  }).on('error', (reason, item) => {
    assert.ok(
      /The `url` property must be a valid URL/.test(reason),
      'Error event called with correct reason');
    assert.equal(item, bookmarks[1], 'returns input that failed in event');
    errorCount++;
  }).on('end', items => {
    assert.equal(dataCount, 2, 'data event called twice');
    assert.equal(errorCount, 1, 'error event called once');
    assert.equal(items.length, bookmarks.length, 'all items should be in result');
    assert.equal(items[0].toString(), '[object Bookmark]',
      'should be a saved instance');
    assert.equal(items[2].toString(), '[object Bookmark]',
      'should be a saved instance');
    assert.equal(items[1], bookmarks[1], 'should be original, unsaved object');

    search({ query: 'moz' }).on('end', items => {
      assert.equal(items.length, 2, 'only two items were successfully saved');
      bookmarks[1].url = 'http://moz2.com/';
      dataCount = errorCount = 0;
      save(bookmarks).on('data', bookmark => {
        dataCount++;
      }).on('error', reason => errorCount++)
      .on('end', items => {
        assert.equal(items.length, 3, 'all 3 items saved');
        assert.equal(dataCount, 3, '3 data events called');
        assert.equal(errorCount, 0, 'no error events called');
        search({ query: 'moz' }).on('end', items => {
          assert.equal(items.length, 3, 'only 3 items saved');
          items.map(item =>
            assert.ok(/moz\d\.com/.test(item.url), 'correct item'))
          done();
        });
      });
    });
  });
};

exports.testSaveDucktypes = function (assert, done) {
  save({
    title: 'moz',
    url: 'http://mozilla.org',
    type: 'bookmark'
  }).on('data', (bookmark) => {
    compareWithHost(assert, bookmark);
    done();
  });
};

exports.testSaveDucktypesParent = function (assert, done) {
  let folder = { title: 'myfolder', type: 'group' };
  let bookmark = { title: 'mozzie', url: 'http://moz.com', group: folder, type: 'bookmark' };
  let sep = { type: 'separator', group: folder };
  save([sep, bookmark]).on('end', (res) => {
    compareWithHost(assert, res[0]);
    compareWithHost(assert, res[1]);
    assert.equal(res[0].group.title, 'myfolder', 'parent is ducktyped group');
    assert.equal(res[1].group.title, 'myfolder', 'parent is ducktyped group');
    done();
  });
};

/*
 * Tests the scenario where the original bookmark item is resaved
 * and does not have an ID or an updated date, but should still be
 * mapped to the item it created previously
 */
exports.testResaveOriginalItemMapping = function (assert, done) {
  let bookmark = Bookmark({ title: 'moz', url: 'http://moz.org' });
  save(bookmark).on('data', newBookmark => {
    bookmark.title = 'new moz';
    save(bookmark).on('data', newNewBookmark => {
      assert.equal(newBookmark.id, newNewBookmark.id, 'should be the same bookmark item');
      assert.equal(bmsrv.getItemTitle(newBookmark.id), 'new moz', 'should have updated title');
      done();
    });
  });
};

exports.testCreateMultipleBookmarks = function (assert, done) {
  let data = [
    Bookmark({title: 'bm1', url: 'http://bm1.com'}),
    Bookmark({title: 'bm2', url: 'http://bm2.com'}),
    Bookmark({title: 'bm3', url: 'http://bm3.com'}),
  ];
  save(data).on('data', function (bookmark, input) {
    let stored = data.filter(({title}) => title === bookmark.title)[0];
    assert.equal(input, stored, 'input is original input item');
    assert.equal(bookmark.title, stored.title, 'titles match');
    assert.equal(bookmark.url, stored.url, 'urls match');
    compareWithHost(assert, bookmark);
  }).on('end', function (bookmarks) {
    assert.equal(bookmarks.length, 3, 'all bookmarks returned');
    done();
  });
};

exports.testCreateImplicitParent = function (assert, done) {
  let folder = Group({ title: 'my parent' });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: folder }),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: folder }),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: folder })
  ];
  save(bookmarks).on('data', function (bookmark) {
    if (bookmark.type === 'bookmark') {
      assert.equal(bookmark.group.title, folder.title, 'parent is linked');
      compareWithHost(assert, bookmark);
    } else if (bookmark.type === 'group') {
      assert.equal(bookmark.group.id, UNSORTED.id, 'parent ID of group is correct');
      compareWithHost(assert, bookmark);
    }
  }).on('end', function (results) {
    assert.equal(results.length, 3, 'results should only hold explicit saves');
    done();
  });
};

exports.testCreateExplicitParent = function (assert, done) {
  let folder = Group({ title: 'my parent' });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: folder }),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: folder }),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: folder })
  ];
  save(bookmarks.concat(folder)).on('data', function (bookmark) {
    if (bookmark.type === 'bookmark') {
      assert.equal(bookmark.group.title, folder.title, 'parent is linked');
      compareWithHost(assert, bookmark);
    } else if (bookmark.type === 'group') {
      assert.equal(bookmark.group.id, UNSORTED.id, 'parent ID of group is correct');
      compareWithHost(assert, bookmark);
    }
  }).on('end', function () {
    done();
  });
};

exports.testCreateNested = function (assert, done) {
  let topFolder = Group({ title: 'top', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder }),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder }),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder })
  ];
  let dataEventCount = 0;
  save(bookmarks).on('data', function (bookmark) {
    if (bookmark.type === 'bookmark') {
      assert.equal(bookmark.group.title, midFolder.title, 'parent is linked');
    } else if (bookmark.title === 'top') {
      assert.equal(bookmark.group.id, MENU.id, 'parent ID of top group is correct');
    } else {
      assert.equal(bookmark.group.title, topFolder.title, 'parent title of middle group is correct');
    }
    dataEventCount++;
    compareWithHost(assert, bookmark);
  }).on('end', () => {
    assert.equal(dataEventCount, 5, 'data events for all saves have occurred');
    assert.ok('end event called');
    done();
  });
};

/*
 * Was a scenario when implicitly saving a bookmark that was already created,
 * it was not being properly fetched and attempted to recreate
 */
exports.testAddingToExistingParent = function (assert, done) {
  let group = { type: 'group', title: 'mozgroup' };
  let bookmarks = [
    { title: 'moz1', url: 'http://moz1.com', type: 'bookmark', group: group },
    { title: 'moz2', url: 'http://moz2.com', type: 'bookmark', group: group },
    { title: 'moz3', url: 'http://moz3.com', type: 'bookmark', group: group }
  ],
  firstBatch, secondBatch;

  saveP(bookmarks).then(data => {
    firstBatch = data;
    return saveP([
      { title: 'moz4', url: 'http://moz4.com', type: 'bookmark', group: group },
      { title: 'moz5', url: 'http://moz5.com', type: 'bookmark', group: group }
    ]);
  }, assert.fail).then(data => {
    secondBatch = data;
    assert.equal(firstBatch[0].group.id, secondBatch[0].group.id,
      'successfully saved to the same parent');
    done();
  }, assert.fail);
};

exports.testUpdateParent = function (assert, done) {
  let group = { type: 'group', title: 'mozgroup' };
  saveP(group).then(item => {
    item[0].title = 'mozgroup-resave';
    return saveP(item[0]);
  }).then(item => {
    assert.equal(item[0].title, 'mozgroup-resave', 'group saved successfully');
    done();
  });
};

exports.testUpdateSeparator = function (assert, done) {
  let sep = [Separator(), Separator(), Separator()];
  saveP(sep).then(item => {
    item[0].index = 2;
    return saveP(item[0]);
  }).then(item => {
    assert.equal(item[0].index, 2, 'updated index of separator');
    done();
  });
};

exports.testPromisedSave = function (assert, done) {
  let topFolder = Group({ title: 'top', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder}),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder}),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder})
  ];
  let first, second, third;
  saveP(bookmarks).then(bms => {
    first = bms.filter(b => b.title === 'moz1')[0];
    second = bms.filter(b => b.title === 'moz2')[0];
    third = bms.filter(b => b.title === 'moz3')[0];
    assert.equal(first.index, 0);
    assert.equal(second.index, 1);
    assert.equal(third.index, 2);
    first.index = 3;
    return saveP(first);
  }).then(() => {
    assert.equal(bmsrv.getItemIndex(first.id), 2, 'properly moved bookmark');
    assert.equal(bmsrv.getItemIndex(second.id), 0, 'other bookmarks adjusted');
    assert.equal(bmsrv.getItemIndex(third.id), 1, 'other bookmarks adjusted');
    done();
  });
};

exports.testPromisedErrorSave = function (assert, done) {
  let bookmarks = [
    { title: 'moz1', url: 'http://moz1.com', type: 'bookmark'},
    { title: 'moz2', url: 'invalidurl', type: 'bookmark'},
    { title: 'moz3', url: 'http://moz3.com', type: 'bookmark'}
  ];
  saveP(bookmarks).then(invalidResolve, reason => {
    assert.ok(
      /The `url` property must be a valid URL/.test(reason),
      'Error event called with correct reason');

    bookmarks[1].url = 'http://moz2.com';
    return saveP(bookmarks);
  }).then(res => {
    return searchP({ query: 'moz' });
  }).then(res => {
    assert.equal(res.length, 3, 'all 3 should be saved upon retry');
    res.map(item => assert.ok(/moz\d\.com/.test(item.url), 'correct item'));
    done();
  }, invalidReject);
};

exports.testMovingChildren = function (assert, done) {
  let topFolder = Group({ title: 'top', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder}),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder}),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder})
  ];
  save(bookmarks).on('end', bms => {
    let first = bms.filter(b => b.title === 'moz1')[0];
    let second = bms.filter(b => b.title === 'moz2')[0];
    let third = bms.filter(b => b.title === 'moz3')[0];
    assert.equal(first.index, 0);
    assert.equal(second.index, 1);
    assert.equal(third.index, 2);
    /* When moving down in the same container we take
     * into account the removal of the original item. If you want
     * to move from index X to index Y > X you must use
     * moveItem(id, folder, Y + 1)
     */
    first.index = 3;
    save(first).on('end', () => {
      assert.equal(bmsrv.getItemIndex(first.id), 2, 'properly moved bookmark');
      assert.equal(bmsrv.getItemIndex(second.id), 0, 'other bookmarks adjusted');
      assert.equal(bmsrv.getItemIndex(third.id), 1, 'other bookmarks adjusted');
      done();
    });
  });
};

exports.testMovingChildrenNewFolder = function (assert, done) {
  let topFolder = Group({ title: 'top', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let newFolder = Group({ title: 'new', group: MENU });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder}),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder}),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder})
  ];
  save(bookmarks).on('end', bms => {
    let first = bms.filter(b => b.title === 'moz1')[0];
    let second = bms.filter(b => b.title === 'moz2')[0];
    let third = bms.filter(b => b.title === 'moz3')[0];
    let definedMidFolder = first.group;
    let definedNewFolder;
    first.group = newFolder;
    assert.equal(first.index, 0);
    assert.equal(second.index, 1);
    assert.equal(third.index, 2);
    save(first).on('data', (data) => {
      if (data.type === 'group') definedNewFolder = data;
    }).on('end', (moved) => {
      assert.equal(bmsrv.getItemIndex(second.id), 0, 'other bookmarks adjusted');
      assert.equal(bmsrv.getItemIndex(third.id), 1, 'other bookmarks adjusted');
      assert.equal(bmsrv.getItemIndex(first.id), 0, 'properly moved bookmark');
      assert.equal(bmsrv.getFolderIdForItem(first.id), definedNewFolder.id,
        'bookmark has new parent');
      assert.equal(bmsrv.getFolderIdForItem(second.id), definedMidFolder.id,
        'sibling bookmarks did not move');
      assert.equal(bmsrv.getFolderIdForItem(third.id), definedMidFolder.id,
        'sibling bookmarks did not move');
      done();
    });
  });
};

exports.testRemoveFunction = function (assert) {
  let topFolder = Group({ title: 'new', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder}),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder}),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder})
  ];
  remove([midFolder, topFolder].concat(bookmarks)).map(item => {
    assert.equal(item.remove, true, 'remove toggled `remove` property to true');
  });
};

exports.testRemove = function (assert, done) {
  let id;
  createBookmarkItem().then(data => {
    id = data.id;
    compareWithHost(assert, data); // ensure bookmark exists
    save(remove(data)).on('data', (res) => {
      assert.pass('data event should be called');
      assert.ok(!res, 'response should be empty');
    }).on('end', () => {
      assert.throws(function () {
        bmsrv.getItemTitle(id);
      }, 'item should no longer exist');
      done();
    });
  });
};

/*
 * Tests recursively removing children when removing a group
 */
exports.testRemoveAllChildren = function (assert, done) {
  let topFolder = Group({ title: 'new', group: MENU });
  let midFolder = Group({ title: 'middle', group: topFolder });
  let bookmarks = [
    Bookmark({ title: 'moz1', url: 'http://moz1.com', group: midFolder}),
    Bookmark({ title: 'moz2', url: 'http://moz2.com', group: midFolder}),
    Bookmark({ title: 'moz3', url: 'http://moz3.com', group: midFolder})
  ];

  let saved = [];
  save(bookmarks).on('data', (data) => saved.push(data)).on('end', () => {
    save(remove(topFolder)).on('end', () => {
      assert.equal(saved.length, 5, 'all items should have been saved');
      saved.map((item) => {
        assert.throws(function () {
          bmsrv.getItemTitle(item.id);
        }, 'item should no longer exist');
      });
      done();
    });
  });
};

exports.testResolution = function (assert, done) {
  let firstSave, secondSave;
  createBookmarkItem().then((item) => {
    firstSave = item;
    assert.ok(item.updated, 'bookmark has updated time');
    item.title = 'my title';
    // Ensure delay so a different save time is set
    return delayed(item);
  }).then(saveP)
  .then(items => {
    let item = items[0];
    secondSave = item;
    assert.ok(firstSave.updated < secondSave.updated, 'snapshots have different update times');
    firstSave.title = 'updated title';
    return saveP(firstSave, { resolve: (mine, theirs) => {
      assert.equal(mine.title, 'updated title', 'correct data for my object');
      assert.equal(theirs.title, 'my title', 'correct data for their object');
      assert.equal(mine.url, theirs.url, 'other data is equal');
      assert.equal(mine.group, theirs.group, 'other data is equal');
      assert.ok(mine !== firstSave, 'instance is not passed in');
      assert.ok(theirs !== secondSave, 'instance is not passed in');
      assert.equal(mine.toString(), '[object Object]', 'serialized objects');
      assert.equal(theirs.toString(), '[object Object]', 'serialized objects');
      mine.title = 'a new title';
      return mine;
    }});
  }).then((results) => {
    let result = results[0];
    assert.equal(result.title, 'a new title', 'resolve handles results');
    done();
  });
};

/*
 * Same as the resolution test, but with the 'unsaved' snapshot
 */
exports.testResolutionMapping = function (assert, done) {
  let bookmark = Bookmark({ title: 'moz', url: 'http://bookmarks4life.com/' });
  let saved;
  saveP(bookmark).then(data => {
    saved = data[0];
    saved.title = 'updated title';
    // Ensure a delay for different updated times
    return delayed(saved);
  }).then(saveP)
  .then(() => {
    bookmark.title = 'conflicting title';
    return saveP(bookmark, { resolve: (mine, theirs) => {
      assert.equal(mine.title, 'conflicting title', 'correct data for my object');
      assert.equal(theirs.title, 'updated title', 'correct data for their object');
      assert.equal(mine.url, theirs.url, 'other data is equal');
      assert.equal(mine.group, theirs.group, 'other data is equal');
      assert.ok(mine !== bookmark, 'instance is not passed in');
      assert.ok(theirs !== saved, 'instance is not passed in');
      assert.equal(mine.toString(), '[object Object]', 'serialized objects');
      assert.equal(theirs.toString(), '[object Object]', 'serialized objects');
      mine.title = 'a new title';
      return mine;
    }});
  }).then((results) => {
    let result = results[0];
    assert.equal(result.title, 'a new title', 'resolve handles results');
    done();
  });
};

exports.testUpdateTags = function (assert, done) {
  createBookmarkItem({ tags: ['spidermonkey'] }).then(bookmark => {
    bookmark.tags.add('jagermonkey');
    bookmark.tags.add('ionmonkey');
    bookmark.tags.delete('spidermonkey');
    save(bookmark).on('data', saved => {
      assert.equal(saved.tags.size, 2, 'should have 2 tags');
      assert.ok(saved.tags.has('jagermonkey'), 'should have added tag');
      assert.ok(saved.tags.has('ionmonkey'), 'should have added tag');
      assert.ok(!saved.tags.has('spidermonkey'), 'should not have removed tag');
      done();
    });
  });
};

/*
 * View `createBookmarkTree` in `./places-helper.js` to see
 * expected tree construction
 */

exports.testSearchByGroupSimple = function (assert, done) {
  createBookmarkTree().then(() => {
     // In initial release of Places API, groups can only be queried
     // via a 'simple query', which is one folder set, and no other
     // parameters
    return searchP({ group: UNSORTED });
  }).then(results => {
    let groups = results.filter(({type}) => type === 'group');
    assert.equal(groups.length, 2, 'returns folders');
    assert.equal(results.length, 7,
      'should return all bookmarks and folders under UNSORTED');
    assert.equal(groups[0].toString(), '[object Group]', 'returns instance');
    return searchP({
      group: groups.filter(({title}) => title === 'mozgroup')[0]
    });
  }).then(results => {
    let groups = results.filter(({type}) => type === 'group');
    assert.equal(groups.length, 1, 'returns one subfolder');
    assert.equal(results.length, 6,
      'returns all children bookmarks/folders');
    assert.ok(results.filter(({url}) => url === 'http://w3schools.com/'),
      'returns nested children');
    done();
  }).then(null, assert.fail);
};

exports.testSearchByGroupComplex = function (assert, done) {
  let mozgroup;
  createBookmarkTree().then(results => {
    mozgroup = results.filter(({title}) => title === 'mozgroup')[0];
    return searchP({ group: mozgroup, query: 'javascript' });
  }).then(results => {
    assert.equal(results.length, 1, 'only one javascript result under mozgroup');
    assert.equal(results[0].url, 'http://w3schools.com/', 'correct result');
    return searchP({ group: mozgroup, url: '*.mozilla.org' });
  }).then(results => {
    assert.equal(results.length, 2, 'expected results');
    assert.ok(
      !results.filter(({url}) => /developer.mozilla/.test(url)).length,
      'does not find results from other folders');
    done();
  }, assert.fail);
};

exports.testSearchEmitters = function (assert, done) {
  createBookmarkTree().then(() => {
    let count = 0;
    search({ tags: ['mozilla', 'firefox'] }).on('data', data => {
      assert.ok(/mozilla|firefox/.test(data.title), 'one of the correct items');
      assert.ok(data.tags.has('firefox'), 'has firefox tag');
      assert.ok(data.tags.has('mozilla'), 'has mozilla tag');
      assert.equal(data + '', '[object Bookmark]', 'returns bookmark');
      count++;
    }).on('end', data => {
      assert.equal(count, 3, 'data event was called for each item');
      assert.equal(data.length, 3,
        'should return two bookmarks that have both mozilla AND firefox');
      assert.equal(data[0].title, 'mozilla.com', 'returns correct bookmark');
      assert.equal(data[1].title, 'mozilla.org', 'returns correct bookmark');
      assert.equal(data[2].title, 'firefox', 'returns correct bookmark');
      assert.equal(data[0] + '', '[object Bookmark]', 'returns bookmarks');
      done();
    });
  });
};

exports.testSearchTags = function (assert, done) {
  createBookmarkTree().then(() => {
    // AND tags
    return searchP({ tags: ['mozilla', 'firefox'] });
  }).then(data => {
    assert.equal(data.length, 3,
      'should return two bookmarks that have both mozilla AND firefox');
    assert.equal(data[0].title, 'mozilla.com', 'returns correct bookmark');
    assert.equal(data[1].title, 'mozilla.org', 'returns correct bookmark');
    assert.equal(data[2].title, 'firefox', 'returns correct bookmark');
    assert.equal(data[0] + '', '[object Bookmark]', 'returns bookmarks');
    return searchP([{tags: ['firefox']}, {tags: ['javascript']}]);
  }).then(data => {
    // OR tags
    assert.equal(data.length, 6,
      'should return all bookmarks with firefox OR javascript tag');
    done();
  });
};

/*
 * Tests 4 scenarios
 * '*.mozilla.com'
 * 'mozilla.com'
 * 'http://mozilla.com/'
 * 'http://mozilla.com/*'
 */
exports.testSearchURL = function (assert, done) {
  createBookmarkTree().then(() => {
    return searchP({ url: 'mozilla.org' });
  }).then(data => {
    assert.equal(data.length, 2, 'only URLs with host domain');
    assert.equal(data[0].url, 'http://mozilla.org/');
    assert.equal(data[1].url, 'http://mozilla.org/thunderbird/');
    return searchP({ url: '*.mozilla.org' });
  }).then(data => {
    assert.equal(data.length, 3, 'returns domain and when host is other than domain');
    assert.equal(data[0].url, 'http://mozilla.org/');
    assert.equal(data[1].url, 'http://mozilla.org/thunderbird/');
    assert.equal(data[2].url, 'http://developer.mozilla.org/en-US/');
    return searchP({ url: 'http://mozilla.org' });
  }).then(data => {
    assert.equal(data.length, 1, 'only exact URL match');
    assert.equal(data[0].url, 'http://mozilla.org/');
    return searchP({ url: 'http://mozilla.org/*' });
  }).then(data => {
    assert.equal(data.length, 2, 'only URLs that begin with query');
    assert.equal(data[0].url, 'http://mozilla.org/');
    assert.equal(data[1].url, 'http://mozilla.org/thunderbird/');
    return searchP([{ url: 'mozilla.org' }, { url: 'component.fm' }]);
  }).then(data => {
    assert.equal(data.length, 3, 'returns URLs that match EITHER query');
    assert.equal(data[0].url, 'http://mozilla.org/');
    assert.equal(data[1].url, 'http://mozilla.org/thunderbird/');
    assert.equal(data[2].url, 'http://component.fm/');
  }).then(() => {
    done();
  });
};

/*
 * Searches url, title, tags
 */
exports.testSearchQuery = function (assert, done) {
  createBookmarkTree().then(() => {
    return searchP({ query: 'thunder' });
  }).then(data => {
    assert.equal(data.length, 3);
    assert.equal(data[0].title, 'mozilla.com', 'query matches tag, url, or title');
    assert.equal(data[1].title, 'mozilla.org', 'query matches tag, url, or title');
    assert.equal(data[2].title, 'thunderbird', 'query matches tag, url, or title');
    return searchP([{ query: 'rust' }, { query: 'component' }]);
  }).then(data => {
    // rust OR component
    assert.equal(data.length, 3);
    assert.equal(data[0].title, 'mozilla.com', 'query matches tag, url, or title');
    assert.equal(data[1].title, 'mozilla.org', 'query matches tag, url, or title');
    assert.equal(data[2].title, 'web audio components', 'query matches tag, url, or title');
    return searchP([{ query: 'moz', tags: ['javascript']}]);
  }).then(data => {
    assert.equal(data.length, 1);
    assert.equal(data[0].title, 'mdn',
      'only one item matches moz query AND has a javascript tag');
  }).then(() => {
    done();
  });
};

/*
 * Test caching on bulk calls.
 * Each construction of a bookmark item snapshot results in
 * the recursive lookup of parent groups up to the root groups --
 * ensure that the appropriate instances equal each other, and no duplicate
 * fetches are called
 *
 * Implementation-dependent, this checks the host event `sdk-places-bookmarks-get`,
 * and if implementation changes, this could increase or decrease
 */

exports.testCaching = function (assert, done) {
  let count = 0;
  let stream = filter(request, ({event}) =>
    /sdk-places-bookmarks-get/.test(event));
  on(stream, 'data', handle);

  let group = { type: 'group', title: 'mozgroup' };
  let bookmarks = [
    { title: 'moz1', url: 'http://moz1.com', type: 'bookmark', group: group },
    { title: 'moz2', url: 'http://moz2.com', type: 'bookmark', group: group },
    { title: 'moz3', url: 'http://moz3.com', type: 'bookmark', group: group }
  ];

  /*
   * Use timeout in tests since the platform calls are synchronous
   * and the counting event shim may not have occurred yet
   */

  saveP(bookmarks).then(() => {
    assert.equal(count, 0, 'all new items and root group, no fetches should occur');
    count = 0;
    return saveP([
      { title: 'moz4', url: 'http://moz4.com', type: 'bookmark', group: group },
      { title: 'moz5', url: 'http://moz5.com', type: 'bookmark', group: group }
    ]);
  // Test `save` look-up
  }).then(() => {
    assert.equal(count, 1, 'should only look up parent once');
    count = 0;
    return searchP({ query: 'moz' });
  }).then(results => {
    // Should query for each bookmark (5) from the query (id -> data),
    // their parent during `construct` (1) and the root shouldn't
    // require a lookup
    assert.equal(count, 6, 'lookup occurs once for each item and parent');
    off(stream, 'data', handle);
    done();
  });

  function handle ({data}) count++
};

/*
 * Search Query Options
 */

exports.testSearchCount = function (assert, done) {
  let max = 8;
  createBookmarkTree()
  .then(testCount(1))
  .then(testCount(2))
  .then(testCount(3))
  .then(testCount(5))
  .then(testCount(10))
  .then(() => {
    done();
  });

  function testCount (n) {
    return function () {
      return searchP({}, { count: n }).then(results => {
        if (n > max) n = max;
        assert.equal(results.length, n,
          'count ' + n + ' returns ' + n + ' results');
      });
    };
  }
};

exports.testSearchSort = function (assert, done) {
  let urls = [
    'http://mozilla.com/', 'http://webaud.io/', 'http://mozilla.com/webfwd/',
    'http://developer.mozilla.com/', 'http://bandcamp.com/'
  ];

  saveP(
    urls.map(url =>
      Bookmark({ url: url, title: url.replace(/http:\/\/|\//g,'')}))
  ).then(() => {
    return searchP({}, { sort: 'title' });
  }).then(results => {
    checkOrder(results, [4,3,0,2,1]);
    return searchP({}, { sort: 'title', descending: true });
  }).then(results => {
    checkOrder(results, [1,2,0,3,4]);
    return searchP({}, { sort: 'url' });
  }).then(results => {
    checkOrder(results, [4,3,0,2,1]);
    return searchP({}, { sort: 'url', descending: true });
  }).then(results => {
    checkOrder(results, [1,2,0,3,4]);
    return addVisits(['http://mozilla.com/', 'http://mozilla.com']);
  }).then(() =>
    saveP(Bookmark({ url: 'http://github.com', title: 'github.com' }))
  ).then(() => addVisits('http://bandcamp.com/'))
  .then(() => searchP({ query: 'webfwd' }))
  .then(results => {
    results[0].title = 'new title for webfwd';
    return saveP(results[0]);
  })
  .then(() =>
    searchP({}, { sort: 'visitCount' })
  ).then(results => {
    assert.equal(results[5].url, 'http://mozilla.com/',
      'last entry is the highest visit count');
    return searchP({}, { sort: 'visitCount', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://mozilla.com/',
      'first entry is the highest visit count');
    return searchP({}, { sort: 'date' });
  }).then(results => {
    assert.equal(results[5].url, 'http://bandcamp.com/',
      'latest visited should be first');
    return searchP({}, { sort: 'date', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://bandcamp.com/',
      'latest visited should be at the end');
    return searchP({}, { sort: 'dateAdded' });
  }).then(results => {
    assert.equal(results[5].url, 'http://github.com/',
     'last added should be at the end');
    return searchP({}, { sort: 'dateAdded', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://github.com/',
      'last added should be first');
    return searchP({}, { sort: 'lastModified' });
  }).then(results => {
    assert.equal(results[5].url, 'http://mozilla.com/webfwd/',
      'last modified should be last');
    return searchP({}, { sort: 'lastModified', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://mozilla.com/webfwd/',
      'last modified should be first');
  }).then(() => {
    done();
  });

  function checkOrder (results, nums) {
    assert.equal(results.length, nums.length, 'expected return count');
    for (let i = 0; i < nums.length; i++) {
      assert.equal(results[i].url, urls[nums[i]], 'successful order');
    }
  }
};

exports.testSearchComplexQueryWithOptions = function (assert, done) {
  createBookmarkTree().then(() => {
    return searchP([
      { tags: ['rust'], url: '*.mozilla.org' },
      { tags: ['javascript'], query: 'mozilla' }
    ], { sort: 'title' });
  }).then(results => {
    let expected = [
      'http://developer.mozilla.org/en-US/',
      'http://mozilla.org/'
    ];
    for (let i = 0; i < expected.length; i++)
      assert.equal(results[i].url, expected[i], 'correct ordering and item');
    done();
  });
};

exports.testCheckSaveOrder = function (assert, done) {
  let group = Group({ title: 'mygroup' });
  let bookmarks = [
    Bookmark({ url: 'http://url1.com', title: 'url1', group: group }),
    Bookmark({ url: 'http://url2.com', title: 'url2', group: group }),
    Bookmark({ url: 'http://url3.com', title: 'url3', group: group }),
    Bookmark({ url: 'http://url4.com', title: 'url4', group: group }),
    Bookmark({ url: 'http://url5.com', title: 'url5', group: group })
  ];
  saveP(bookmarks).then(results => {
    for (let i = 0; i < bookmarks.length; i++)
      assert.equal(results[i].url, bookmarks[i].url,
        'correct ordering of bookmark results');
    done();
  });
};

before(exports, (name, assert, done) => resetPlaces(done));
after(exports, (name, assert, done) => resetPlaces(done));

function saveP () {
  return promisedEmitter(save.apply(null, Array.slice(arguments)));
}

function searchP () {
  return promisedEmitter(search.apply(null, Array.slice(arguments)));
}

function delayed (value, ms) {
  let { promise, resolve } = defer();
  setTimeout(() => resolve(value), ms || 10);
  return promise;
}
