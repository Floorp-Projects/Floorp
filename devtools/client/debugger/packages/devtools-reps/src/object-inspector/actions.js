/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { GripProperties, Node, Props, ReduxAction } from "./types";

const { loadItemProperties } = require("./utils/load-properties");
const { getPathExpression, getValue } = require("./utils/node");
const { getLoadedProperties, getActors, getWatchpoints } = require("./reducer");

type Dispatch = ReduxAction => void;

type ThunkArg = {
  getState: () => {},
  dispatch: Dispatch,
};

/**
 * This action is responsible for expanding a given node, which also means that
 * it will call the action responsible to fetch properties.
 */
function nodeExpand(node: Node, actor) {
  return async ({ dispatch, getState }: ThunkArg) => {
    dispatch({ type: "NODE_EXPAND", data: { node } });
    dispatch(nodeLoadProperties(node, actor));
  };
}

function nodeCollapse(node: Node) {
  return {
    type: "NODE_COLLAPSE",
    data: { node },
  };
}

/*
 * This action checks if we need to fetch properties, entries, prototype and
 * symbols for a given node. If we do, it will call the appropriate ObjectClient
 * functions.
 */
function nodeLoadProperties(node: Node, actor) {
  return async ({ dispatch, client, getState }: ThunkArg) => {
    const state = getState();
    const loadedProperties = getLoadedProperties(state);
    if (loadedProperties.has(node.path)) {
      return;
    }

    try {
      const properties = await loadItemProperties(
        node,
        client,
        loadedProperties
      );

      dispatch(nodePropertiesLoaded(node, actor, properties));
    } catch (e) {
      console.error(e);
    }
  };
}

function nodePropertiesLoaded(
  node: Node,
  actor?: string,
  properties: GripProperties
) {
  return {
    type: "NODE_PROPERTIES_LOADED",
    data: { node, actor, properties },
  };
}

/*
 * This action adds a property watchpoint to an object
 */
function addWatchpoint(item, watchpoint: string) {
  return async function({ dispatch, client }: ThunkArgs) {
    const { parent, name } = item;
    const object = getValue(parent);
    if (!object) {
      return;
    }

    const path = parent.path;
    const property = name;
    const label = getPathExpression(item);
    const actor = object.actor;

    await client.addWatchpoint(object, property, label, watchpoint);

    dispatch({
      type: "SET_WATCHPOINT",
      data: { path, watchpoint, property, actor },
    });
  };
}

/*
 * This action removes a property watchpoint from an object
 */
function removeWatchpoint(item) {
  return async function({ dispatch, client }: ThunkArgs) {
    const object = getValue(item.parent);
    const property = item.name;
    const path = item.parent.path;
    const actor = object.actor;

    await client.removeWatchpoint(object, property);

    dispatch({
      type: "REMOVE_WATCHPOINT",
      data: { path, property, actor },
    });
  };
}

function closeObjectInspector() {
  return ({ dispatch, getState, client }: ThunkArg) =>
    releaseActors(getState(), client, dispatch);
}

/*
 * This action is dispatched when the `roots` prop, provided by a consumer of
 * the ObjectInspector (inspector, console, …), is modified. It will clean the
 * internal state properties (expandedPaths, loadedProperties, …) and release
 * the actors consumed with the previous roots.
 * It takes a props argument which reflects what is passed by the upper-level
 * consumer.
 */
function rootsChanged(props: Props) {
  return ({ dispatch, client, getState }: ThunkArg) => {
    releaseActors(getState(), client, dispatch);
    dispatch({
      type: "ROOTS_CHANGED",
      data: props,
    });
  };
}

async function releaseActors(state, client, dispatch) {
  const actors = getActors(state);
  if (actors.size === 0) {
    return;
  }

  const watchpoints = getWatchpoints(state);
  let released = false;
  for (const actor of actors) {
    // Watchpoints are stored in object actors.
    // If we release the actor we lose the watchpoint.
    if (!watchpoints.has(actor)) {
      await client.releaseActor(actor);
      released = true;
    }
  }

  if (released) {
    dispatch({
      type: "RELEASED_ACTORS",
      data: { actors },
    });
  }
}

function invokeGetter(
  node: Node,
  targetGrip: object,
  receiverId: string | null,
  getterName: string
) {
  return async ({ dispatch, client, getState }: ThunkArg) => {
    try {
      const objectClient = client.createObjectClient(targetGrip);
      const result = await objectClient.getPropertyValue(
        getterName,
        receiverId
      );
      dispatch({
        type: "GETTER_INVOKED",
        data: {
          node,
          result,
        },
      });
    } catch (e) {
      console.error(e);
    }
  };
}

module.exports = {
  closeObjectInspector,
  invokeGetter,
  nodeExpand,
  nodeCollapse,
  nodeLoadProperties,
  nodePropertiesLoaded,
  rootsChanged,
  addWatchpoint,
  removeWatchpoint,
};
