/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function createInitial() {
  return {
    identity: {},
    values: {},
  };
}

export function insertResources(state, resources) {
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
      throw new Error(
        `Resource "${id}" already exists, cannot insert ${JSON.stringify(
          resource
        )}`
      );
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

export function removeResources(state, resources) {
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

export function updateResources(state, resources) {
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

export function makeIdentity() {
  return {};
}

export function getValidatedResource(state, id) {
  const value = state.values[id];
  const identity = state.identity[id];
  if ((value && !identity) || (!value && identity)) {
    throw new Error(
      `Resource state corrupt: ${id} has mismatched value and identity`
    );
  }

  return value ? state : null;
}

export function getResourceValues(state) {
  return state.values;
}
