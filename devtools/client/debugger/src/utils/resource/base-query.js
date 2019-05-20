/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getResourceValues,
  getResourcePair,
  makeIdentity,
  type ResourceBound,
  type Id,
  type ResourceState,
  type ResourceValues,
  type ResourceIdentity,
} from "./core";
import { arrayShallowEqual } from "./compare";

export type ResourceQuery<R: ResourceBound, Args, Reduced> = (
  ResourceState<R>,
  Args
) => Reduced;

export type QueryFilter<R: ResourceBound, Args> = (
  ResourceValues<R>,
  Args
) => Array<Id<R>>;

export type QueryMap<R: ResourceBound, Args, Mapped> =
  | QueryMapNoArgs<R, Mapped>
  | QueryMapWithArgs<R, Args, Mapped>;
export type QueryMapNoArgs<R: ResourceBound, Mapped> = {
  +needsArgs?: false,
  (R, ResourceIdentity): Mapped,
};
export type QueryMapWithArgs<R: ResourceBound, Args, Mapped> = {
  +needsArgs: true,
  (R, ResourceIdentity, Args): Mapped,
};

export type QueryReduce<R: ResourceBound, Args, Mapped, Reduced> = (
  $ReadOnlyArray<Mapped>,
  $ReadOnlyArray<Id<R>>,
  Args
) => Reduced;

export type QueryContext<Args> = {
  args: Args,
  identMap: WeakMap<ResourceIdentity, ResourceIdentity>,
};
export type QueryResult<Mapped, Reduced> = {
  mapped: Array<Mapped>,
  reduced: Reduced,
};
export type QueryResultCompare<Reduced> = (Reduced, Reduced) => boolean;

export type QueryCacheHandler<R: ResourceBound, Args, Mapped, Reduced> = (
  ResourceState<R>,
  QueryContext<Args>,
  QueryResult<Mapped, Reduced> | null
) => QueryResult<Mapped, Reduced>;

export type QueryCache<R: ResourceBound, Args, Mapped, Reduced> = (
  handler: QueryCacheHandler<R, Args, Mapped, Reduced>
) => ResourceQuery<R, Args, Reduced>;

export function makeMapWithArgs<R: ResourceBound, Args, Mapped>(
  map: (R, ResourceIdentity, Args) => Mapped
): QueryMapWithArgs<R, Args, Mapped> {
  const wrapper = (resource, identity, args) => map(resource, identity, args);
  wrapper.needsArgs = true;
  return wrapper;
}

export function makeResourceQuery<R: ResourceBound, Args, Mapped, Reduced>({
  cache,
  filter,
  map,
  reduce,
  resultCompare,
}: {|
  cache: QueryCache<R, Args, Mapped, Reduced>,
  filter: QueryFilter<R, Args>,
  map: QueryMap<R, Args, Mapped>,
  reduce: QueryReduce<R, Args, Mapped, Reduced>,
  resultCompare: QueryResultCompare<Reduced>,
|}): ResourceQuery<R, Args, Reduced> {
  const loadResource = makeResourceMapper(map);

  return cache((state, context, existing) => {
    const ids = filter(getResourceValues(state), context.args);
    const mapped = ids.map(id => loadResource(state, id, context));

    if (existing && arrayShallowEqual(existing.mapped, mapped)) {
      // If the items are exactly the same as the existing ones, we return
      // early to reuse the existing result.
      return existing;
    }

    const reduced = reduce(mapped, ids, context.args);

    if (existing && resultCompare(existing.reduced, reduced)) {
      return existing;
    }

    return { mapped, reduced };
  });
}

type ResourceLoader<R: ResourceBound, Args, Mapped> = (
  ResourceState<R>,
  Id<R>,
  QueryContext<Args>
) => Mapped;

function makeResourceMapper<R: ResourceBound, Args, Mapped>(
  map: QueryMap<R, Args, Mapped>
): ResourceLoader<R, Args, Mapped> {
  return map.needsArgs
    ? makeResourceArgsMapper(map)
    : makeResourceNoArgsMapper(map);
}

/**
 * Resources loaded when things care about arguments need to be given a
 * special ResourceIdentity object that correlates with both the resource
 * _and_ the arguments being passed to the query. That means they need extra
 * logic when loading those resources.
 */
function makeResourceArgsMapper<R: ResourceBound, Args, Mapped>(
  map: QueryMapWithArgs<R, Args, Mapped>
): ResourceLoader<R, Args, Mapped> {
  const mapper = (value, identity, context) =>
    map(value, getIdentity(context.identMap, identity), context.args);
  return (state, id, context) => getCachedResource(state, id, context, mapper);
}

function makeResourceNoArgsMapper<R: ResourceBound, Args, Mapped>(
  map: QueryMapNoArgs<R, Mapped>
): ResourceLoader<R, Args, Mapped> {
  const mapper = (value, identity, context) => map(value, identity);
  return (state, id, context) => getCachedResource(state, id, context, mapper);
}

function getCachedResource<R: ResourceBound, Args, Mapped>(
  state: ResourceState<R>,
  id: Id<R>,
  context: QueryContext<Args>,
  map: (
    value: R,
    identity: ResourceIdentity,
    context: QueryContext<Args>
  ) => Mapped
): Mapped {
  const pair = getResourcePair(state, id);
  if (!pair) {
    throw new Error(`Resource ${id} does not exist`);
  }

  return map(pair.value, pair.identity, context);
}

function getIdentity(
  identMap: WeakMap<ResourceIdentity, ResourceIdentity>,
  identity: ResourceIdentity
) {
  let ident = identMap.get(identity);
  if (!ident) {
    ident = makeIdentity();
    identMap.set(identity, ident);
  }

  return ident;
}
