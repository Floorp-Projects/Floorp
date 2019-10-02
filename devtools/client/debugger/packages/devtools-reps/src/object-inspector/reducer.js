/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function initialState(overrides) {
  return {
    expandedPaths: new Set(),
    loadedProperties: new Map(),
    evaluations: new Map(),
    actors: new Set(),
    watchpoints: new Map(),
    ...overrides,
  };
}

function reducer(state = initialState(), action = {}) {
  const { type, data } = action;

  const cloneState = overrides => ({ ...state, ...overrides });

  if (type === "NODE_EXPAND") {
    return cloneState({
      expandedPaths: new Set(state.expandedPaths).add(data.node.path),
    });
  }

  if (type === "NODE_COLLAPSE") {
    const expandedPaths = new Set(state.expandedPaths);
    expandedPaths.delete(data.node.path);
    return cloneState({ expandedPaths });
  }

  if (type == "SET_WATCHPOINT") {
    const { watchpoint, property, path } = data;
    const obj = state.loadedProperties.get(path);

    return cloneState({
      loadedProperties: new Map(state.loadedProperties).set(
        path,
        updateObject(obj, property, watchpoint)
      ),
      watchpoints: new Map(state.watchpoints).set(data.actor, data.watchpoint),
    });
  }

  if (type === "REMOVE_WATCHPOINT") {
    const { path, property, actor } = data;
    const obj = state.loadedProperties.get(path);
    const watchpoints = new Map(state.watchpoints);
    watchpoints.delete(actor);

    return cloneState({
      loadedProperties: new Map(state.loadedProperties).set(
        path,
        updateObject(obj, property, null)
      ),
      watchpoints: watchpoints,
    });
  }

  if (type === "NODE_PROPERTIES_LOADED") {
    return cloneState({
      actors: data.actor
        ? new Set(state.actors || []).add(data.actor)
        : state.actors,
      loadedProperties: new Map(state.loadedProperties).set(
        data.node.path,
        action.data.properties
      ),
    });
  }

  if (type === "RELEASED_ACTORS") {
    return onReleasedActorsAction(state, action);
  }

  if (type === "ROOTS_CHANGED") {
    return cloneState();
  }

  if (type === "GETTER_INVOKED") {
    return cloneState({
      actors: data.actor
        ? new Set(state.actors || []).add(data.result.from)
        : state.actors,
      evaluations: new Map(state.evaluations).set(data.node.path, {
        getterValue:
          data.result &&
          data.result.value &&
          (data.result.value.throw || data.result.value.return),
      }),
    });
  }

  // NOTE: we clear the state on resume because otherwise the scopes pane
  // would be out of date. Bug 1514760
  if (type === "RESUME" || type == "NAVIGATE") {
    return initialState({ watchpoints: state.watchpoints });
  }

  return state;
}

/**
 * Reducer function for the "RELEASED_ACTORS" action.
 */
function onReleasedActorsAction(state, action) {
  const { data } = action;

  if (state.actors && state.actors.size > 0 && data.actors.length > 0) {
    return state;
  }

  for (const actor of data.actors) {
    state.actors.delete(actor);
  }

  return {
    ...state,
    actors: new Set(state.actors || []),
  };
}

function updateObject(obj, property, watchpoint) {
  return {
    ...obj,
    ownProperties: {
      ...obj.ownProperties,
      [property]: {
        ...obj.ownProperties[property],
        watchpoint,
      },
    },
  };
}

function getObjectInspectorState(state) {
  return state.objectInspector;
}

function getExpandedPaths(state) {
  return getObjectInspectorState(state).expandedPaths;
}

function getExpandedPathKeys(state) {
  return [...getExpandedPaths(state).keys()];
}

function getActors(state) {
  return getObjectInspectorState(state).actors;
}

function getWatchpoints(state) {
  return getObjectInspectorState(state).watchpoints;
}

function getLoadedProperties(state) {
  return getObjectInspectorState(state).loadedProperties;
}

function getLoadedPropertyKeys(state) {
  return [...getLoadedProperties(state).keys()];
}

function getEvaluations(state) {
  return getObjectInspectorState(state).evaluations;
}

const selectors = {
  getActors,
  getWatchpoints,
  getEvaluations,
  getExpandedPathKeys,
  getExpandedPaths,
  getLoadedProperties,
  getLoadedPropertyKeys,
};

Object.defineProperty(module.exports, "__esModule", {
  value: true,
});
module.exports = selectors;
module.exports.default = reducer;
