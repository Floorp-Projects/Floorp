/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeResourceQuery } from "./base-query";

import {
  queryCacheWeak,
  queryCacheShallow,
  queryCacheStrict,
} from "./query-cache";

import { memoizeResourceShallow } from "./memoize";
import { shallowEqual } from "./compare";

export function filterAllIds(values) {
  return Object.keys(values);
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export function makeWeakQuery({ filter, map, reduce }) {
  return makeResourceQuery({
    cache: queryCacheWeak,
    filter,
    map: memoizeResourceShallow(map),
    reduce,
    resultCompare: shallowEqual,
  });
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export function makeShallowQuery({ filter, map, reduce }) {
  return makeResourceQuery({
    cache: queryCacheShallow,
    filter,
    map: memoizeResourceShallow(map),
    reduce,
    resultCompare: shallowEqual,
  });
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export function makeStrictQuery({ filter, map, reduce }) {
  return makeResourceQuery({
    cache: queryCacheStrict,
    filter,
    map: memoizeResourceShallow(map),
    reduce,
    resultCompare: shallowEqual,
  });
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export function makeIdQuery(map) {
  return makeWeakQuery({
    filter: (state, ids) => ids,
    map: (r, identity) => map(r, identity),
    reduce: items => items.slice(),
  });
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export function makeLoadQuery(map) {
  return makeWeakQuery({
    filter: (state, ids) => ids,
    map: (r, identity) => map(r, identity),
    reduce: reduceMappedArrayToObject,
  });
}

/**
 * Create a query function that accepts an argument and can filter the
 * resource items to a subset before mapping each reduced resource.
 */
export function makeFilterQuery(filter, map) {
  return makeWeakQuery({
    filter: (values, args) => {
      const ids = [];
      for (const id of Object.keys(values)) {
        if (filter(values[id], args)) {
          ids.push(id);
        }
      }
      return ids;
    },
    map,
    reduce: reduceMappedArrayToObject,
  });
}

/**
 * Create a query function that accepts an argument and can filter the
 * resource items to a subset before mapping each resulting resource.
 */
export function makeReduceQuery(map, reduce) {
  return makeShallowQuery({
    filter: filterAllIds,
    map,
    reduce,
  });
}

/**
 * Create a query function that accepts an argument and can filter the
 * resource items to a subset before mapping each resulting resource.
 */
export function makeReduceAllQuery(map, reduce) {
  return makeStrictQuery({
    filter: filterAllIds,
    map,
    reduce,
  });
}

function reduceMappedArrayToObject(items, ids, args) {
  return items.reduce((acc, item, i) => {
    acc[ids[i]] = item;
    return acc;
  }, {});
}
