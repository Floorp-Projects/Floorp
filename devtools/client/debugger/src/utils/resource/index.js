/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export {
  createInitial,
  insertResources,
  removeResources,
  updateResources,
} from "./core";

export {
  hasResource,
  getResourceIds,
  getResource,
  getMappedResource,
} from "./selector";

export { makeResourceQuery, makeMapWithArgs } from "./base-query";

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

export {
  queryCacheWeak,
  queryCacheShallow,
  queryCacheStrict,
} from "./query-cache";

export { memoizeResourceShallow } from "./memoize";
