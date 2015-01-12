/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "*"
  }
};

const { Cc, Ci } = require('chrome');
const browserHistory = Cc["@mozilla.org/browser/nav-history-service;1"].
                       getService(Ci.nsIBrowserHistory);
const asyncHistory = Cc["@mozilla.org/browser/history;1"].
                     getService(Ci.mozIAsyncHistory);
const bmsrv = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                        getService(Ci.nsINavBookmarksService);
const taggingService = Cc["@mozilla.org/browser/tagging-service;1"].
                       getService(Ci.nsITaggingService);
const ios = Cc['@mozilla.org/network/io-service;1'].
            getService(Ci.nsIIOService);
const { query } = require('./host-query');
const {
  defer, all, resolve, promised, reject
} = require('../../core/promise');
const { request, response } = require('../../addon/host');
const { send } = require('../../addon/events');
const { on, emit } = require('../../event/core');
const { filter } = require('../../event/utils');
const { URL, isValidURI } = require('../../url');
const { newURI } = require('../../url/utils');

const DEFAULT_INDEX = bmsrv.DEFAULT_INDEX;
const UNSORTED_ID = bmsrv.unfiledBookmarksFolder;
const ROOT_FOLDERS = [
  bmsrv.unfiledBookmarksFolder, bmsrv.toolbarFolder,
  bmsrv.tagsFolder, bmsrv.bookmarksMenuFolder
];

const EVENT_MAP = {
  'sdk-places-bookmarks-create': createBookmarkItem,
  'sdk-places-bookmarks-save': saveBookmarkItem,
  'sdk-places-bookmarks-last-updated': getBookmarkLastUpdated,
  'sdk-places-bookmarks-get': getBookmarkItem,
  'sdk-places-bookmarks-remove': removeBookmarkItem,
  'sdk-places-bookmarks-get-all': getAllBookmarks,
  'sdk-places-bookmarks-get-children': getChildren
};

function typeMap (type) {
  if (typeof type === 'number') {
    if (bmsrv.TYPE_BOOKMARK === type) return 'bookmark';
    if (bmsrv.TYPE_FOLDER === type) return 'group';
    if (bmsrv.TYPE_SEPARATOR === type) return 'separator';
  } else {
    if ('bookmark' === type) return bmsrv.TYPE_BOOKMARK;
    if ('group' === type) return bmsrv.TYPE_FOLDER;
    if ('separator' === type) return bmsrv.TYPE_SEPARATOR;
  }
}

function getBookmarkLastUpdated ({id})
  resolve(bmsrv.getItemLastModified(id))
exports.getBookmarkLastUpdated;

function createBookmarkItem (data) {
  let error;

  if (data.group == null) data.group = UNSORTED_ID;
  if (data.index == null) data.index = DEFAULT_INDEX;

  if (data.type === 'group')
    data.id = bmsrv.createFolder(
      data.group, data.title, data.index
    );
  else if (data.type === 'separator')
    data.id = bmsrv.insertSeparator(
      data.group, data.index
    );
  else
    data.id = bmsrv.insertBookmark(
      data.group, newURI(data.url), data.index, data.title
    );

  // In the event where default or no index is provided (-1),
  // query the actual index for the response
  if (data.index === -1)
    data.index = bmsrv.getItemIndex(data.id);

  try {
    data.updated = bmsrv.getItemLastModified(data.id);
  }
  catch (e) {
    console.exception(e);
  }

  return tag(data, true).then(() => data);
}
exports.createBookmarkItem = createBookmarkItem;

function saveBookmarkItem (data) {
  let id = data.id;
  if (!id)
    reject('Item is missing id');

  let group = bmsrv.getFolderIdForItem(id);
  let index = bmsrv.getItemIndex(id);
  let type = bmsrv.getItemType(id);
  let title = typeMap(type) !== 'separator' ?
    bmsrv.getItemTitle(id) :
    undefined;
  let url = typeMap(type) === 'bookmark' ?
    bmsrv.getBookmarkURI(id).spec :
    undefined;

  if (url != data.url)
    bmsrv.changeBookmarkURI(id, newURI(data.url));
  else if (typeMap(type) === 'bookmark')
    data.url = url;

  if (title != data.title)
    bmsrv.setItemTitle(id, data.title);
  else if (typeMap(type) !== 'separator')
    data.title = title;

  if (data.group && data.group !== group)
    bmsrv.moveItem(id, data.group, data.index || -1);
  else if (data.index != null && data.index !== index) {
    // We use moveItem here instead of setItemIndex
    // so we don't have to manage the indicies of the siblings
    bmsrv.moveItem(id, group, data.index);
  } else if (data.index == null)
    data.index = index;

  data.updated = bmsrv.getItemLastModified(data.id);

  return tag(data).then(() => data);
}
exports.saveBookmarkItem = saveBookmarkItem;

function removeBookmarkItem (data) {
  let id = data.id;

  if (!id)
    reject('Item is missing id');

  bmsrv.removeItem(id);
  return resolve(null);
}
exports.removeBookmarkItem = removeBookmarkItem;

function getBookmarkItem (data) {
  let id = data.id;

  if (!id)
    reject('Item is missing id');

  let type = bmsrv.getItemType(id);

  data.type = typeMap(type);

  if (type === bmsrv.TYPE_BOOKMARK || type === bmsrv.TYPE_FOLDER)
    data.title = bmsrv.getItemTitle(id);

  if (type === bmsrv.TYPE_BOOKMARK) {
    data.url = bmsrv.getBookmarkURI(id).spec;
    // Should be moved into host-tags as a method
    data.tags = taggingService.getTagsForURI(newURI(data.url), {});
  }

  data.group = bmsrv.getFolderIdForItem(id);
  data.index = bmsrv.getItemIndex(id);
  data.updated = bmsrv.getItemLastModified(data.id);

  return resolve(data);
}
exports.getBookmarkItem = getBookmarkItem;

function getAllBookmarks () {
  return query({}, { queryType: 1 }).then(bookmarks =>
    all(bookmarks.map(getBookmarkItem)));
}
exports.getAllBookmarks = getAllBookmarks;

function getChildren ({ id }) {
  if (typeMap(bmsrv.getItemType(id)) !== 'group') return [];
  let ids = [];
  for (let i = 0; ids[ids.length - 1] !== -1; i++)
    ids.push(bmsrv.getIdForItemAt(id, i));
  ids.pop();
  return all(ids.map(id => getBookmarkItem({ id: id })));
}
exports.getChildren = getChildren;

/*
 * Hook into host
 */

let reqStream = filter(request, (data) => /sdk-places-bookmarks/.test(data.event));
on(reqStream, 'data', ({ event, id, data }) => {
  if (!EVENT_MAP[event]) return;

  let resData = { id: id, event: event };

  promised(EVENT_MAP[event])(data).
  then(res => resData.data = res, e => resData.error = e).
  then(() => emit(response, 'data', resData));
});

function tag (data, isNew) {
  // If a new item, we can skip checking what other tags
  // are on the item
  if (data.type !== 'bookmark') {
    return resolve();
  }
  else if (!isNew) {
    return send('sdk-places-tags-get-tags-by-url', { url: data.url })
      .then(tags => {
        return send('sdk-places-tags-untag', {
          tags: tags.filter(tag => !~data.tags.indexOf(tag)),
          url: data.url
        });
      }).then(() => send('sdk-places-tags-tag', {
        url: data.url, tags: data.tags
      }));
  }
  else if (data.tags && data.tags.length) {
    return send('sdk-places-tags-tag', { url: data.url, tags: data.tags });
  }
  else
    return resolve();
}

