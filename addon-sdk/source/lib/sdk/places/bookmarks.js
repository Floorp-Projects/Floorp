/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable",
  "engines": {
    "Firefox": "*",
    "SeaMonkey": "*"
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
const { send } = require('../addon/events');
const { defer, reject, all, resolve, promised } = require('../core/promise');
const { EventTarget } = require('../event/target');
const { emit } = require('../event/core');
const { identity, defer:async } = require('../lang/functional');
const { extend, merge } = require('../util/object');
const { fromIterator } = require('../util/array');
const {
  constructTree, fetchItem, createQuery,
  isRootGroup, createQueryOptions
} = require('./utils');
const {
  bookmarkContract, groupContract, separatorContract
} = require('./contract');
const bmsrv = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);

/*
 * Mapping of uncreated bookmarks with their created
 * counterparts
 */
const itemMap = new WeakMap();

/*
 * Constant used by nsIHistoryQuery; 1 is a bookmark query
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryQueryOptions
 */
const BOOKMARK_QUERY = 1;

/*
 * Bookmark Item classes
 */

const Bookmark = Class({
  extends: [
    bookmarkContract.properties(identity)
  ],
  initialize: function initialize (options) {
    merge(this, bookmarkContract(extend(defaults, options)));
  },
  type: 'bookmark',
  toString: () => '[object Bookmark]'
});
exports.Bookmark = Bookmark;

const Group = Class({
  extends: [
    groupContract.properties(identity)
  ],
  initialize: function initialize (options) {
    // Don't validate if root group
    if (isRootGroup(options))
      merge(this, options);
    else
      merge(this, groupContract(extend(defaults, options)));
  },
  type: 'group',
  toString: () => '[object Group]'
});
exports.Group = Group;

const Separator = Class({
  extends: [
    separatorContract.properties(identity)
  ],
  initialize: function initialize (options) {
    merge(this, separatorContract(extend(defaults, options)));
  },
  type: 'separator',
  toString: () => '[object Separator]'
});
exports.Separator = Separator;

/*
 * Functions
 */

function save (items, options) {
  items = [].concat(items);
  options = options || {};
  let emitter = EventTarget();
  let results = [];
  let errors = [];
  let root = constructTree(items);
  let cache = new Map();

  let isExplicitSave = item => !!~items.indexOf(item);
  // `walk` returns an aggregate promise indicating the completion
  // of the `commitItem` on each node, not whether or not that
  // commit was successful

  // Force this to be async, as if a ducktype fails validation,
  // the promise implementation will fire an error event, which will
  // not trigger the handler as it's not yet bound
  //
  // Can remove after `Promise.jsm` is implemented in Bug 881047,
  // which will guarantee next tick execution
  async(() => root.walk(preCommitItem).then(commitComplete))();

  function preCommitItem ({value:item}) {
    // Do nothing if tree root, default group (unsavable),
    // or if it's a dependency and not explicitly saved (in the list
    // of items to be saved), and not needed to be saved
    if (item === null || // node is the tree root
        isRootGroup(item) ||
        (getId(item) && !isExplicitSave(item)))
      return;

    return promised(validate)(item)
      .then(() => commitItem(item, options))
      .then(data => construct(data, cache))
      .then(savedItem => {
        // If item was just created, make a map between
        // the creation object and created object,
        // so we can reference the item that doesn't have an id
        if (!getId(item))
          saveId(item, savedItem.id);

        // Emit both the processed item, and original item
        // so a mapping can be understood in handler
        emit(emitter, 'data', savedItem, item);
       
        // Push to results iff item was explicitly saved
        if (isExplicitSave(item))
          results[items.indexOf(item)] = savedItem;
      }, reason => {
        // Force reason to be a string for consistency
        reason = reason + '';
        // Emit both the reason, and original item
        // so a mapping can be understood in handler
        emit(emitter, 'error', reason + '', item);
        // Store unsaved item in results list
        results[items.indexOf(item)] = item;
        errors.push(reason);
      });
  }

  // Called when traversal of the node tree is completed and all
  // items have been committed
  function commitComplete () {
    emit(emitter, 'end', results);
  }

  return emitter;
}
exports.save = save;

function search (queries, options) {
  queries = [].concat(queries);
  let emitter = EventTarget();
  let cache = new Map();
  let queryObjs = queries.map(createQuery.bind(null, BOOKMARK_QUERY));
  let optionsObj = createQueryOptions(BOOKMARK_QUERY, options);

  // Can remove after `Promise.jsm` is implemented in Bug 881047,
  // which will guarantee next tick execution
  async(() => {
    send('sdk-places-query', { queries: queryObjs, options: optionsObj })
      .then(handleQueryResponse);
  })();
    
  function handleQueryResponse (data) {
    let deferreds = data.map(item => {
      return construct(item, cache).then(bookmark => {
        emit(emitter, 'data', bookmark);
        return bookmark;
      }, reason => {
        emit(emitter, 'error', reason);
        errors.push(reason);
      });
    });

    all(deferreds).then(data => {
      emit(emitter, 'end', data);
    }, () => emit(emitter, 'end', []));
  }

  return emitter;
}
exports.search = search;

function remove (items) {
  return [].concat(items).map(item => {
    item.remove = true;
    return item;
  });
}

exports.remove = remove;

/*
 * Internal Utilities
 */

function commitItem (item, options) {
  // Get the item's ID, or getId it's saved version if it exists
  let id = getId(item);
  let data = normalize(item);
  let promise;

  data.id = id;

  if (!id) {
    promise = send('sdk-places-bookmarks-create', data);
  } else if (item.remove) {
    promise = send('sdk-places-bookmarks-remove', { id: id });
  } else {
    promise = send('sdk-places-bookmarks-last-updated', {
      id: id
    }).then(function (updated) {
      // If attempting to save an item that is not the 
      // latest snapshot of a bookmark item, execute
      // the resolution function
      if (updated !== item.updated && options.resolve)
        return fetchItem(id)
          .then(options.resolve.bind(null, data));
      else
        return data;
    }).then(send.bind(null, 'sdk-places-bookmarks-save'));
  }

  return promise;
}

/*
 * Turns a bookmark item into a plain object,
 * converts `tags` from Set to Array, group instance to an id
 */
function normalize (item) {
  let data = merge({}, item);
  // Circumvent prototype property of `type`
  delete data.type;
  data.type = item.type;
  data.tags = [];
  if (item.tags) {
    data.tags = fromIterator(item.tags);
  }
  data.group = getId(data.group) || exports.UNSORTED.id;

  return data;
}

/*
 * Takes a data object and constructs a BookmarkItem instance
 * of it, recursively generating parent instances as well.
 *
 * Pass in a `cache` Map to reuse instances of
 * bookmark items to reduce overhead;
 * The cache object is a map of id to a deferred with a 
 * promise that resolves to the bookmark item.
 */
function construct (object, cache, forced) {
  let item = instantiate(object);
  let deferred = defer();

  // Item could not be instantiated
  if (!item)
    return resolve(null);

  // Return promise for item if found in the cache,
  // and not `forced`. `forced` indicates that this is the construct
  // call that should not read from cache, but should actually perform
  // the construction, as it was set before several async calls
  if (cache.has(item.id) && !forced)
    return cache.get(item.id).promise;
  else if (cache.has(item.id))
    deferred = cache.get(item.id);
  else
    cache.set(item.id, deferred);

  // When parent group is found in cache, use
  // the same deferred value
  if (item.group && cache.has(item.group)) {
    cache.get(item.group).promise.then(group => {
      item.group = group;
      deferred.resolve(item);
    });

  // If not in the cache, and a root group, return
  // the premade instance
  } else if (rootGroups.get(item.group)) {
    item.group = rootGroups.get(item.group);
    deferred.resolve(item);

  // If not in the cache or a root group, fetch the parent
  } else {
    cache.set(item.group, defer());
    fetchItem(item.group).then(group => {
      return construct(group, cache, true);
    }).then(group => {
      item.group = group;
      deferred.resolve(item);
    }, deferred.reject);
  }

  return deferred.promise;
}

function instantiate (object) {
  if (object.type === 'bookmark')
    return Bookmark(object);
  if (object.type === 'group')
    return Group(object);
  if (object.type === 'separator')
    return Separator(object);
  return null;
}

/**
 * Validates a bookmark item; will throw an error if ininvalid,
 * to be used with `promised`. As bookmark items check on their class,
 * this only checks ducktypes
 */
function validate (object) {
  if (!isDuckType(object)) return true;
  let contract = object.type === 'bookmark' ? bookmarkContract :
                 object.type === 'group' ? groupContract :
                 object.type === 'separator' ? separatorContract :
                 null;
  if (!contract) {
    throw Error('No type specified');
  }

  // If object has a property set, and undefined,
  // manually override with default as it'll fail otherwise
  let withDefaults = Object.keys(defaults).reduce((obj, prop) => {
    if (obj[prop] == null) obj[prop] = defaults[prop];
    return obj;
  }, extend(object));

  contract(withDefaults);
}

function isDuckType (item) {
  return !(item instanceof Bookmark) &&
    !(item instanceof Group) &&
    !(item instanceof Separator);
}

function saveId (unsaved, id) {
  itemMap.set(unsaved, id);
}

// Fetches an item's ID from itself, or from the mapped items
function getId (item) {
  return typeof item === 'number' ? item :
    item ? item.id || itemMap.get(item) :
    null;
}

/*
 * Set up the default, root groups
 */

var defaultGroupMap = {
  MENU: bmsrv.bookmarksMenuFolder,
  TOOLBAR: bmsrv.toolbarFolder,
  UNSORTED: bmsrv.unfiledBookmarksFolder
};

var rootGroups = new Map();

for (let i in defaultGroupMap) {
  let group = Object.freeze(Group({ title: i, id: defaultGroupMap[i] }));
  rootGroups.set(defaultGroupMap[i], group);
  exports[i] = group;
}

var defaults = {
  group: exports.UNSORTED,
  index: -1
};
