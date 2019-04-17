/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  AUDIT,
  FETCH_CHILDREN,
  HIGHLIGHT,
  RESET,
  SELECT,
} = require("../constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return new Map();
}

/**
 * Maintain a cache of received accessibles responses from the backend.
 */
function accessibles(state = getInitialState(), action) {
  switch (action.type) {
    case FETCH_CHILDREN:
      return onReceiveChildren(state, action);
    case HIGHLIGHT:
    case SELECT:
      return onReceiveAncestry(state, action);
    case AUDIT:
      return onAudit(state, action);
    case RESET:
      return getInitialState();
    default:
      return state;
  }
}

function getActorID(accessible) {
  return accessible.actorID || (accessible._form && accessible._form.actor);
}

/**
 * If accessible is cached recursively remove all its children and remove itself
 * from cache.
 * @param {Map}    cache      Previous state maintaining a cache of previously
 *                            fetched accessibles.
 * @param {Object} accessible Accessible object to remove from cache.
 */
function cleanupChild(cache, accessible) {
  const actorID = getActorID(accessible);
  const cached = cache.get(actorID);
  if (!cached) {
    return;
  }

  for (const child of cached.children) {
    cleanupChild(cache, child);
  }

  cache.delete(actorID);
}

/**
 * Determine if accessible in cache is stale. Accessible object is stale if its
 * cached children array has the size other than the value of its childCount
 * property that updates on accessible actor event.
 * @param {Map}    cache      Previous state maintaining a cache of previously
 *                             fetched accessibles.
 * @param {Object} accessible Accessible object to test for staleness.
 */
function staleChildren(cache, accessible) {
  const cached = cache.get(getActorID(accessible));
  if (!cached) {
    return false;
  }

  return cached.children.length !== accessible.childCount;
}

function updateChildrenCache(cache, accessible, children) {
  const actorID = getActorID(accessible);

  if (cache.has(actorID)) {
    const cached = cache.get(actorID);
    for (const child of cached.children) {
      // If exhisting children cache includes an accessible that is not present
      // any more or if child accessible is stale remove it and all its children
      // from cache.
      if (!children.includes(child) || staleChildren(cache, child)) {
        cleanupChild(cache, child);
      }
    }
    cached.children = children;
    cache.set(actorID, cached);
  } else {
    cache.set(actorID, { children });
  }

  return cache;
}

function updateAncestry(cache, ancestry) {
  ancestry.forEach(({ accessible, children }) =>
    updateChildrenCache(cache, accessible, children));

  return cache;
}

/**
 * Handles fetching of accessible children.
 * @param {Map}     cache  Previous state maintaining a cache of previously
 *                         fetched accessibles.
 * @param {Object}  action Redux action object.
 * @return {Object} updated state
 */
function onReceiveChildren(cache, action) {
  const { error, accessible, response: children } = action;
  if (!error) {
    return updateChildrenCache(new Map(cache), accessible, children);
  }

  if (accessible.actorID) {
    console.warn(`Error fetching children: `, accessible, error);
    return cache;
  }

  const newCache = new Map(cache);
  cleanupChild(newCache, accessible);
  return newCache;
}

function onReceiveAncestry(cache, action) {
  const { error, response: ancestry } = action;
  if (error) {
    console.warn(`Error fetching ancestry: `, error);
    return cache;
  }

  return updateAncestry(new Map(cache), ancestry);
}

function onAudit(cache, action) {
  const { error, response: ancestries } = action;
  if (error) {
    console.warn(`Error performing an audit: `, error);
    return cache;
  }

  const newCache = new Map(cache);
  ancestries.forEach(ancestry => updateAncestry(newCache, ancestry));

  return newCache;
}

exports.accessibles = accessibles;
