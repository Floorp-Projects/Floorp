/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type Resource<R: ResourceBound> = $ReadOnly<$Exact<R>>;

export type ResourceBound = {
  +id: string,
};
export type Id<R: ResourceBound> = $ElementType<R, "id">;

type ResourceSubset<R: ResourceBound> = $ReadOnly<{
  +id: Id<R>,
  ...$Shape<$Rest<R, { +id: Id<R> }>>,
}>;

export opaque type ResourceIdentity: { [string]: mixed } = {||};
export type ResourceValues<R: ResourceBound> = { [Id<R>]: R };

export opaque type ResourceState<R: ResourceBound> = {
  identity: { [Id<R>]: ResourceIdentity },
  values: ResourceValues<R>,
};

export function createInitial<R: ResourceBound>(): ResourceState<R> {
  return {
    identity: {},
    values: {},
  };
}

export function insertResources<R: ResourceBound>(
  state: ResourceState<R>,
  resources: $ReadOnlyArray<R>
): ResourceState<R> {
  if (resources.length === 0) {
    return state;
  }

  state = {
    identity: { ...state.identity },
    values: { ...state.values },
  };

  for (const resource of resources) {
    const { id } = resource;
    if (state.identity[id]) {
      throw new Error(`Resource "${id}" already exists, cannot insert`);
    }
    if (state.values[id]) {
      throw new Error(
        `Resource state corrupt: ${id} has value but no identity`
      );
    }

    state.identity[resource.id] = makeIdentity();
    state.values[resource.id] = resource;
  }
  return state;
}

export function removeResources<R: ResourceBound>(
  state: ResourceState<R>,
  resources: $ReadOnlyArray<ResourceSubset<R> | Id<R>>
): ResourceState<R> {
  if (resources.length === 0) {
    return state;
  }

  state = {
    identity: { ...state.identity },
    values: { ...state.values },
  };

  for (let id of resources) {
    if (typeof id !== "string") {
      id = id.id;
    }

    if (!state.identity[id]) {
      throw new Error(`Resource "${id}" does not exists, cannot remove`);
    }
    if (!state.values[id]) {
      throw new Error(
        `Resource state corrupt: ${id} has identity but no value`
      );
    }

    delete state.identity[id];
    delete state.values[id];
  }
  return state;
}

export function updateResources<R: ResourceBound>(
  state: ResourceState<R>,
  resources: $ReadOnlyArray<ResourceSubset<R>>
): ResourceState<R> {
  if (resources.length === 0) {
    return state;
  }

  let didCopyValues = false;

  for (const subset of resources) {
    const { id } = subset;

    if (!state.identity[id]) {
      throw new Error(`Resource "${id}" does not exists, cannot update`);
    }
    if (!state.values[id]) {
      throw new Error(
        `Resource state corrupt: ${id} has identity but no value`
      );
    }

    const existing = state.values[id];
    const updated = {};

    for (const field of Object.keys(subset)) {
      if (field === "id") {
        continue;
      }

      if (subset[field] !== existing[field]) {
        updated[field] = subset[field];
      }
    }

    if (Object.keys(updated).length > 0) {
      if (!didCopyValues) {
        didCopyValues = true;
        state = {
          identity: state.identity,
          values: { ...state.values },
        };
      }

      state.values[id] = { ...existing, ...updated };
    }
  }

  return state;
}

export function makeIdentity(): ResourceIdentity {
  return ({}: any);
}

export function getResourcePair<R: ResourceBound>(
  state: ResourceState<R>,
  id: Id<R>
): {
  value: R,
  identity: ResourceIdentity,
} | null {
  const value = state.values[id];
  const identity = state.identity[id];
  if ((value && !identity) || (!value && identity)) {
    throw new Error(
      `Resource state corrupt: ${id} has mismatched value and identity`
    );
  }

  return value ? { value, identity } : null;
}

export function getResourceValues<R: ResourceBound>(
  state: ResourceState<R>
): ResourceValues<R> {
  return state.values;
}
