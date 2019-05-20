/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ResourceBound, Id, ResourceValues } from "./core";

import {
  makeResourceQuery,
  type ResourceQuery,
  type QueryFilter,
  type QueryMap,
  type QueryReduce,
} from "./base-query";

import {
  queryCacheWeak,
  queryCacheShallow,
  queryCacheStrict,
  type WeakArgsBound,
  type ShallowArgsBound,
} from "./query-cache";

import { memoizeResourceShallow } from "./memoize";
import { shallowEqual } from "./compare";

export function filterAllIds<R: ResourceBound>(
  values: ResourceValues<R>
): Array<Id<R>> {
  return Object.keys(values);
}

/**
 * Create a query function to take a list of IDs and map each Reduceding
 * resource object into a mapped form.
 */
export type WeakQuery<
  R: ResourceBound,
  Args: WeakArgsBound,
  Reduced
> = ResourceQuery<R, Args, Reduced>;
export function makeWeakQuery<
  R: ResourceBound,
  Args: WeakArgsBound,
  Mapped,
  Reduced
>({
  filter,
  map,
  reduce,
}: {|
  filter: QueryFilter<R, Args>,
  map: QueryMap<R, Args, Mapped>,
  reduce: QueryReduce<R, Args, Mapped, Reduced>,
|}): WeakQuery<R, Args, Reduced> {
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
export type ShallowQuery<R: ResourceBound, Args, Reduced> = ResourceQuery<
  R,
  Args,
  Reduced
>;
export function makeShallowQuery<
  R: ResourceBound,
  Args: ShallowArgsBound,
  Mapped,
  Reduced
>({
  filter,
  map,
  reduce,
}: {|
  filter: QueryFilter<R, Args>,
  map: QueryMap<R, Args, Mapped>,
  reduce: QueryReduce<R, Args, Mapped, Reduced>,
|}): ShallowQuery<R, Args, Reduced> {
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
export type StrictQuery<R: ResourceBound, Args, Reduced> = ResourceQuery<
  R,
  Args,
  Reduced
>;
export function makeStrictQuery<R: ResourceBound, Args, Mapped, Reduced>({
  filter,
  map,
  reduce,
}: {|
  filter: QueryFilter<R, Args>,
  map: QueryMap<R, Args, Mapped>,
  reduce: QueryReduce<R, Args, Mapped, Reduced>,
|}): StrictQuery<R, Args, Reduced> {
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
export type IdQuery<R: ResourceBound, Mapped> = WeakQuery<
  R,
  Array<Id<R>>,
  Array<Mapped>
>;
export function makeIdQuery<R: ResourceBound, Mapped>(
  map: QueryMap<R, void, Mapped>
): IdQuery<R, Mapped> {
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
export type LoadQuery<R: ResourceBound, Mapped> = WeakQuery<
  R,
  Array<Id<R>>,
  $ReadOnly<{ [Id<R>]: Mapped }>
>;
export function makeLoadQuery<R: ResourceBound, Mapped>(
  map: QueryMap<R, void, Mapped>
): LoadQuery<R, Mapped> {
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
export type FilterQuery<
  R: ResourceBound,
  Args: WeakArgsBound,
  Mapped
> = WeakQuery<R, Args, $ReadOnly<{ [Id<R>]: Mapped }>>;
export function makeFilterQuery<R: ResourceBound, Args: WeakArgsBound, Mapped>(
  filter: (R, Args) => boolean,
  map: QueryMap<R, Args, Mapped>
): FilterQuery<R, Args, Mapped> {
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
export type ReduceQuery<
  R: ResourceBound,
  Args: ShallowArgsBound,
  Reduced
> = ShallowQuery<R, Args, Reduced>;
export function makeReduceQuery<
  R: ResourceBound,
  Args: ShallowArgsBound,
  Mapped,
  Reduced
>(
  map: QueryMap<R, Args, Mapped>,
  reduce: QueryReduce<R, Args, Mapped, Reduced>
): ReduceQuery<R, Args, Reduced> {
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
export type ReduceAllQuery<R: ResourceBound, Reduced> = ShallowQuery<
  R,
  void,
  Reduced
>;
export function makeReduceAllQuery<R: ResourceBound, Mapped, Reduced>(
  map: QueryMap<R, void, Mapped>,
  reduce: QueryReduce<R, void, Mapped, Reduced>
): ReduceAllQuery<R, Reduced> {
  return makeStrictQuery({
    filter: filterAllIds,
    map,
    reduce,
  });
}

function reduceMappedArrayToObject<Args, ID, Mapped>(
  items: $ReadOnlyArray<Mapped>,
  ids: $ReadOnlyArray<ID>,
  args: Args
): { [ID]: Mapped } {
  return items.reduce((acc: { [ID]: Mapped }, item, i) => {
    acc[ids[i]] = item;
    return acc;
  }, {});
}
