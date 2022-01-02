/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { loadItemProperties } = require("devtools/client/shared/components/object-inspector/utils/load-properties");
const {
  getPathExpression,
  getParentFront,
  getParentGripValue,
  getValue,
  nodeIsBucket,
  getFront,
} = require("devtools/client/shared/components/object-inspector/utils/node");
const { getLoadedProperties, getWatchpoints } = require("devtools/client/shared/components/object-inspector/reducer");

/**
 * This action is responsible for expanding a given node, which also means that
 * it will call the action responsible to fetch properties.
 */
function nodeExpand(node, actor) {
  return async ({ dispatch }) => {
    dispatch({ type: "NODE_EXPAND", data: { node } });
    dispatch(nodeLoadProperties(node, actor));
  };
}

function nodeCollapse(node) {
  return {
    type: "NODE_COLLAPSE",
    data: { node },
  };
}

/*
 * This action checks if we need to fetch properties, entries, prototype and
 * symbols for a given node. If we do, it will call the appropriate ObjectFront
 * functions.
 */
function nodeLoadProperties(node, actor) {
  return async ({ dispatch, client, getState }) => {
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

      // If the client does not have a releaseActor function, it means the actors are
      // handled directly by the consumer, so we don't need to track them.
      if (!client || !client.releaseActor) {
        actor = null;
      }

      dispatch(nodePropertiesLoaded(node, actor, properties));
    } catch (e) {
      console.error(e);
    }
  };
}

function nodePropertiesLoaded(node, actor, properties) {
  return {
    type: "NODE_PROPERTIES_LOADED",
    data: { node, actor, properties },
  };
}

/*
 * This action adds a property watchpoint to an object
 */
function addWatchpoint(item, watchpoint) {
  return async function({ dispatch, client }) {
    const { parent, name } = item;
    let object = getValue(parent);

    if (nodeIsBucket(parent)) {
      object = getValue(parent.parent);
    }

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
  return async function({ dispatch, client }) {
    const { parent, name } = item;
    let object = getValue(parent);

    if (nodeIsBucket(parent)) {
      object = getValue(parent.parent);
    }

    const property = name;
    const path = parent.path;
    const actor = object.actor;

    await client.removeWatchpoint(object, property);

    dispatch({
      type: "REMOVE_WATCHPOINT",
      data: { path, property, actor },
    });
  };
}

function getActorIDs(roots) {
  return (roots || []).reduce((ids, root) => {
    const front = getFront(root);
    return front ? ids.concat(front.actorID) : ids;
  }, []);
}

function closeObjectInspector(roots) {
  return ({ dispatch, getState, client }) => {
    releaseActors(roots, client, dispatch);
  };
}

/*
 * This action is dispatched when the `roots` prop, provided by a consumer of
 * the ObjectInspector (inspector, console, …), is modified. It will clean the
 * internal state properties (expandedPaths, loadedProperties, …) and release
 * the actors consumed with the previous roots.
 * It takes a props argument which reflects what is passed by the upper-level
 * consumer.
 */
function rootsChanged(roots) {
  return ({ dispatch, client, getState }) => {
    releaseActors(roots, client, dispatch);
    dispatch({
      type: "ROOTS_CHANGED",
      data: roots,
    });
  };
}

async function releaseActors(roots, client, dispatch) {
  if (!client || !client.releaseActor) {
    return;
  }

  const actors = getActorIDs(roots);
  await Promise.all(actors.map(client.releaseActor));
}

function invokeGetter(node, receiverId) {
  return async ({ dispatch, client, getState }) => {
    try {
      const objectFront =
        getParentFront(node) ||
        client.createObjectFront(getParentGripValue(node));
      const getterName = node.propertyName || node.name;

      const result = await objectFront.getPropertyValue(getterName, receiverId);
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
