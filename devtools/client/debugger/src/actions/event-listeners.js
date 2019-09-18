/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { uniq, remove } from "lodash";

import {
  getActiveEventListeners,
  getEventListenerExpanded,
  shouldLogEventBreakpoints,
} from "../selectors";

import type { ThunkArgs } from "./types";

async function updateBreakpoints(dispatch, client, newEvents: string[]) {
  dispatch({ type: "UPDATE_EVENT_LISTENERS", active: newEvents });
  await client.setEventListenerBreakpoints(newEvents);
}

async function updateExpanded(dispatch, newExpanded: string[]) {
  dispatch({
    type: "UPDATE_EVENT_LISTENER_EXPANDED",
    expanded: newExpanded,
  });
}

export function addEventListenerBreakpoints(eventsToAdd: string[]) {
  return async ({ dispatch, client, getState }: ThunkArgs) => {
    const activeListenerBreakpoints = await getActiveEventListeners(getState());

    const newEvents = uniq([...eventsToAdd, ...activeListenerBreakpoints]);

    await updateBreakpoints(dispatch, client, newEvents);
  };
}

export function removeEventListenerBreakpoints(eventsToRemove: string[]) {
  return async ({ dispatch, client, getState }: ThunkArgs) => {
    const activeListenerBreakpoints = await getActiveEventListeners(getState());

    const newEvents = remove(
      activeListenerBreakpoints,
      event => !eventsToRemove.includes(event)
    );

    await updateBreakpoints(dispatch, client, newEvents);
  };
}

export function toggleEventLogging() {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const logEventBreakpoints = !shouldLogEventBreakpoints(getState());
    await client.toggleEventLogging(logEventBreakpoints);
    dispatch({ type: "TOGGLE_EVENT_LISTENERS", logEventBreakpoints });
  };
}

export function addEventListenerExpanded(category: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const expanded = await getEventListenerExpanded(getState());

    const newExpanded = uniq([...expanded, category]);

    await updateExpanded(dispatch, newExpanded);
  };
}

export function removeEventListenerExpanded(category: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const expanded = await getEventListenerExpanded(getState());

    const newExpanded = expanded.filter(expand => expand != category);

    updateExpanded(dispatch, newExpanded);
  };
}

export function getEventListenerBreakpointTypes() {
  return async ({ dispatch, client }: ThunkArgs) => {
    const categories = await client.getEventListenerBreakpointTypes();
    dispatch({ type: "RECEIVE_EVENT_LISTENER_TYPES", categories });
  };
}
