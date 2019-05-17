/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getResourcePair,
  getResourceValues,
  type ResourceState,
  type Id,
  type ResourceBound,
  type ResourceIdentity,
} from "./core";

export type ResourceMap<R: ResourceBound, Mapped> = (
  R,
  ResourceIdentity
) => Mapped;

export function hasResource<R: ResourceBound>(
  state: ResourceState<R>,
  id: Id<R>
): boolean %checks {
  return !!getResourcePair(state, id);
}

export function getResourceIds<R: ResourceBound>(
  state: ResourceState<R>
): Array<Id<R>> {
  return Object.keys(getResourceValues(state));
}

export function getResource<R: ResourceBound>(
  state: ResourceState<R>,
  id: Id<R>
): R {
  const pair = getResourcePair(state, id);
  if (!pair) {
    throw new Error(`Resource ${id} does not exist`);
  }
  return pair.value;
}

export function getMappedResource<R: ResourceBound, Mapped>(
  state: ResourceState<R>,
  id: Id<R>,
  map: ResourceMap<R, Mapped>
): Mapped {
  const pair = getResourcePair(state, id);
  if (!pair) {
    throw new Error(`Resource ${id} does not exist`);
  }

  return map(pair.value, pair.identity);
}
