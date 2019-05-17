/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export {
  createInitial,
  insertResources,
  removeResources,
  updateResources,
} from "./core";
export type {
  Id,
  Resource,
  ResourceBound,
  // Disabled pending eslint-plugin-import bug #1345
  // eslint-disable-next-line import/named
  ResourceState,
  // Disabled pending eslint-plugin-import bug #1345
  // eslint-disable-next-line import/named
  ResourceIdentity,
  ResourceValues,
} from "./core";

export {
  hasResource,
  getResourceIds,
  getResource,
  getMappedResource,
} from "./selector";
export type { ResourceMap } from "./selector";

export { makeResourceQuery, makeMapWithArgs } from "./base-query";
export type {
  ResourceQuery,
  QueryMap,
  QueryMapNoArgs,
  QueryMapWithArgs,
  QueryFilter,
  QueryReduce,
  QueryResultCompare,
} from "./base-query";

export {
  filterAllIds,
  makeWeakQuery,
  makeShallowQuery,
  makeStrictQuery,
  makeIdQuery,
  makeLoadQuery,
  makeFilterQuery,
  makeReduceQuery,
  makeReduceAllQuery,
} from "./query";
export type {
  WeakQuery,
  ShallowQuery,
  StrictQuery,
  IdQuery,
  LoadQuery,
  FilterQuery,
  ReduceQuery,
  ReduceAllQuery,
} from "./query";

export {
  queryCacheWeak,
  queryCacheShallow,
  queryCacheStrict,
} from "./query-cache";
export type { WeakArgsBound, ShallowArgsBound } from "./query-cache";

export { memoizeResourceShallow } from "./memoize";
