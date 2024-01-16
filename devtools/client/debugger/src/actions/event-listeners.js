/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getActiveEventListeners,
  getEventListenerExpanded,
  shouldLogEventBreakpoints,
} from "../selectors/index";

async function updateBreakpoints(dispatch, client, newEvents) {
  await client.setEventListenerBreakpoints(newEvents);
  dispatch({ type: "UPDATE_EVENT_LISTENERS", active: newEvents });
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

    const newEvents = [
      ...new Set([...eventsToAdd, ...activeListenerBreakpoints]),
    ];
    await updateBreakpoints(dispatch, client, newEvents);
  };
}

export function removeEventListenerBreakpoints(eventsToRemove) {
  return async ({ dispatch, client, getState }) => {
    const activeListenerBreakpoints = await getActiveEventListeners(getState());

    const newEvents = activeListenerBreakpoints.filter(
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
    const newExpanded = [...new Set([...expanded, category])];
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
