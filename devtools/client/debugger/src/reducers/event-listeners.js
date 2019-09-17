/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { prefs } from "../utils/prefs";

import type { State } from "./types";
import type {
  EventListenerAction,
  EventListenerActiveList,
  EventListenerCategoryList,
  EventListenerExpandedList,
} from "../actions/types";

export type EventListenersState = {|
  +active: EventListenerActiveList,
  +categories: EventListenerCategoryList,
  +expanded: EventListenerExpandedList,
  +logEventBreakpoints: boolean,
|};

export function initialEventListenerState(): EventListenersState {
  return {
    active: [],
    categories: [],
    expanded: [],
    logEventBreakpoints: prefs.logEventBreakpoints,
  };
}

function update(
  state: EventListenersState = initialEventListenerState(),
  action: EventListenerAction
) {
  switch (action.type) {
    case "UPDATE_EVENT_LISTENERS":
      return { ...state, active: action.active };

    case "RECEIVE_EVENT_LISTENER_TYPES":
      return { ...state, categories: action.categories };

    case "UPDATE_EVENT_LISTENER_EXPANDED":
      return { ...state, expanded: action.expanded };

    case "TOGGLE_EVENT_LISTENERS": {
      const { logEventBreakpoints } = action;
      prefs.logEventBreakpoints = logEventBreakpoints;
      return { ...state, logEventBreakpoints };
    }

    default:
      return state;
  }
}

export function getActiveEventListeners(state: State): EventListenerActiveList {
  return state.eventListenerBreakpoints.active;
}

export function getEventListenerBreakpointTypes(
  state: State
): EventListenerCategoryList {
  return state.eventListenerBreakpoints.categories;
}

export function getEventListenerExpanded(
  state: State
): EventListenerExpandedList {
  return state.eventListenerBreakpoints.expanded;
}

export function shouldLogEventBreakpoints(state: State) {
  return state.eventListenerBreakpoints.logEventBreakpoints;
}

export default update;
