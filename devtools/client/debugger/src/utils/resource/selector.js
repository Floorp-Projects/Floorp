/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getValidatedResource,
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
  return !!getValidatedResource(state, id);
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
  const validatedState = getValidatedResource(state, id);
  if (!validatedState) {
    throw new Error(`Resource ${id} does not exist`);
  }
  return validatedState.values[id];
}

export function getMappedResource<R: ResourceBound, Mapped>(
  state: ResourceState<R>,
  id: Id<R>,
  map: ResourceMap<R, Mapped>
): Mapped {
  const validatedState = getValidatedResource(state, id);
  if (!validatedState) {
    throw new Error(`Resource ${id} does not exist`);
  }

  return map(validatedState.values[id], validatedState.identity[id]);
}
