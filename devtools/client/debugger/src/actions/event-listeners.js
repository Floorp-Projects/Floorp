/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { uniq, remove } from "lodash";

import {
  getActiveEventListeners,
  getEventListenerExpanded,
  shouldLogEventBreakpoints,
} from "../selectors";

async function updateBreakpoints(dispatch, client, newEvents) {
  dispatch({ type: "UPDATE_EVENT_LISTENERS", active: newEvents });
  await client.setEventListenerBreakpoints(newEvents);
}

async function updateExpanded(dispatch, newExpanded) {
  dispatch({
    type: "UPDATE_EVENT_LISTENER_EXPANDED",
    expanded: newExpanded,
  });
}

export function addEventListenerBreakpoints(eventsToAdd) {
  return async ({ dispatch, client, getState }) => {
    const activeListenerBreakpoints = await getActiveEventListeners(getState());

    const newEvents = uniq([...eventsToAdd, ...activeListenerBreakpoints]);

    await updateBreakpoints(dispatch, client, newEvents);
  };
}

export function removeEventListenerBreakpoints(eventsToRemove) {
  return async ({ dispatch, client, getState }) => {
    const activeListenerBreakpoints = await getActiveEventListeners(getState());

    const newEvents = remove(
      activeListenerBreakpoints,
      event => !eventsToRemove.includes(event)
    );

    await updateBreakpoints(dispatch, client, newEvents);
  };
}

export function toggleEventLogging() {
  return async ({ dispatch, getState, client }) => {
    const logEventBreakpoints = !shouldLogEventBreakpoints(getState());
    await client.toggleEventLogging(logEventBreakpoints);
    dispatch({ type: "TOGGLE_EVENT_LISTENERS", logEventBreakpoints });
  };
}

export function addEventListenerExpanded(category) {
  return async ({ dispatch, getState }) => {
    const expanded = await getEventListenerExpanded(getState());

    const newExpanded = uniq([...expanded, category]);

    await updateExpanded(dispatch, newExpanded);
  };
}

export function removeEventListenerExpanded(category) {
  return async ({ dispatch, getState }) => {
    const expanded = await getEventListenerExpanded(getState());

    const newExpanded = expanded.filter(expand => expand != category);

    updateExpanded(dispatch, newExpanded);
  };
}

export function getEventListenerBreakpointTypes() {
  return async ({ dispatch, client }) => {
    const categories = await client.getEventListenerBreakpointTypes();
    dispatch({ type: "RECEIVE_EVENT_LISTENER_TYPES", categories });
  };
}
