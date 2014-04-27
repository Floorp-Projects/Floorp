/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "*"
  }
};

const { Cc, Ci } = require('chrome');
const { Class } = require('../core/heritage');
const { method } = require('../lang/functional');
const { defer, promised, all } = require('../core/promise');
const { send } = require('../addon/events');
const { EventTarget } = require('../event/target');
const { merge } = require('../util/object');
const bmsrv = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);

/*
 * TreeNodes are used to construct dependency trees
 * for BookmarkItems
 */
let TreeNode = Class({
  initialize: function (value) {
    this.value = value;
    this.children = [];
  },
  add: function (values) {
    [].concat(values).forEach(value => {
      this.children.push(value instanceof TreeNode ? value : TreeNode(value));
    });
  },
  get length () {
    let count = 0;
    this.walk(() => count++);
    // Do not count the current node
    return --count;
  },
  get: method(get),
  walk: method(walk),
  toString: function () '[object TreeNode]'
});
exports.TreeNode = TreeNode;

/*
 * Descends down from `node` applying `fn` to each in order.
 * `fn` can return values or promises -- if promise returned,
 * children are not processed until resolved. `fn` is passed 
 * one argument, the current node, `curr`.
 */
function walk (curr, fn) {
  return promised(fn)(curr).then(val => {
    return all(curr.children.map(child => walk(child, fn)));
  });
} 

/*
 * Descends from the TreeNode `node`, returning
 * the node with value `value` if found or `null`
 * otherwise
 */
function get (node, value) {
  if (node.value === value) return node;
  for (let child of node.children) {
    let found = get(child, value);
    if (found) return found;
  }
  return null;
}

/*
 * Constructs a tree of bookmark nodes
 * returning the root (value: null);
 */

function constructTree (items) {
  let root = TreeNode(null);
  items.forEach(treeify.bind(null, root));

  function treeify (root, item) {
    // If node already exists, skip
    let node = root.get(item);
    if (node) return node;
    node = TreeNode(item);

    let parentNode = item.group ? treeify(root, item.group) : root;
    parentNode.add(node);

    return node;
  }

  return root;
}
exports.constructTree = constructTree;

/*
 * Shortcut for converting an id, or an object with an id, into
 * an object with corresponding bookmark data
 */
function fetchItem (item)
  send('sdk-places-bookmarks-get', { id: item.id || item })
exports.fetchItem = fetchItem;

/*
 * Takes an ID or an object with ID and checks it against
 * the root bookmark folders
 */
function isRootGroup (id) {
  id = id && id.id;
  return ~[bmsrv.bookmarksMenuFolder, bmsrv.toolbarFolder,
    bmsrv.unfiledBookmarksFolder
  ].indexOf(id);
}
exports.isRootGroup = isRootGroup;

/*
 * Merges appropriate options into query based off of url
 * 4 scenarios:
 * 
 * 'moz.com' // domain: moz.com, domainIsHost: true
 *    --> 'http://moz.com', 'http://moz.com/thunderbird'
 * '*.moz.com' // domain: moz.com, domainIsHost: false
 *    --> 'http://moz.com', 'http://moz.com/index', 'http://ff.moz.com/test'
 * 'http://moz.com' // url: http://moz.com/, urlIsPrefix: false
 *    --> 'http://moz.com/'
 * 'http://moz.com/*' // url: http://moz.com/, urlIsPrefix: true
 *    --> 'http://moz.com/', 'http://moz.com/thunderbird'
 */

function urlQueryParser (query, url) {
  if (!url) return;
  if (/^https?:\/\//.test(url)) {
    query.uri = url.charAt(url.length - 1) === '/' ? url : url + '/';
    if (/\*$/.test(url)) {
      query.uri = url.replace(/\*$/, '');
      query.uriIsPrefix = true;
    }
  } else {
    if (/^\*/.test(url)) {
      query.domain = url.replace(/^\*\./, '');
      query.domainIsHost = false;
    } else {
      query.domain = url;
      query.domainIsHost = true;
    }
  }
}
exports.urlQueryParser = urlQueryParser;

/*
 * Takes an EventEmitter and returns a promise that
 * aggregates results and handles a bulk resolve and reject
 */

function promisedEmitter (emitter) {
  let { promise, resolve, reject } = defer();
  let errors = [];
  emitter.on('error', error => errors.push(error));
  emitter.on('end', (items) => {
    if (errors.length) reject(errors[0]);
    else resolve(items);
  });
  return promise;
}
exports.promisedEmitter = promisedEmitter;


// https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryQueryOptions
function createQuery (type, query) {
  query = query || {};
  let qObj = {
    searchTerms: query.query
  };
     
  urlQueryParser(qObj, query.url);
  
  // 0 === history
  if (type === 0) {
    // PRTime used by query is in microseconds, not milliseconds
    qObj.beginTime = (query.from || 0) * 1000;
    qObj.endTime = (query.to || new Date()) * 1000;

    // Set reference time to Epoch
    qObj.beginTimeReference = 0;
    qObj.endTimeReference = 0;
  }
  // 1 === bookmarks
  else if (type === 1) {
    qObj.tags = query.tags;
    qObj.folder = query.group && query.group.id;
  } 
  // 2 === unified (not implemented on platform)
  else if (type === 2) {

  }

  return qObj;
}
exports.createQuery = createQuery;

// https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryQueryOptions

const SORT_MAP = {
  title: 1,
  date: 3, // sort by visit date
  url: 5,
  visitCount: 7,
  // keywords currently unsupported
  // keyword: 9,
  dateAdded: 11, // bookmarks only
  lastModified: 13 // bookmarks only
};

function createQueryOptions (type, options) {
  options = options || {};
  let oObj = {};
  oObj.sortingMode = SORT_MAP[options.sort] || 0;
  if (options.descending && options.sort)
    oObj.sortingMode++;

  // Resolve to default sort if ineligible based on query type
  if (type === 0 && // history
      (options.sort === 'dateAdded' || options.sort === 'lastModified'))
    oObj.sortingMode = 0;

  oObj.maxResults = typeof options.count === 'number' ? options.count : 0;

  oObj.queryType = type;

  return oObj;
}
exports.createQueryOptions = createQueryOptions;


function mapBookmarkItemType (type) {
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
exports.mapBookmarkItemType = mapBookmarkItemType;
