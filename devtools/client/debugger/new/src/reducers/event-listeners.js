/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { uniq } from "lodash";

import { asyncStore } from "../utils/prefs";
import type { EventListenerBreakpoints } from "../types";

type OuterState = { eventListenerBreakpoints: EventListenerBreakpoints };

function update(state: EventListenerBreakpoints = [], action: any) {
  switch (action.type) {
    case "ADD_EVENT_LISTENERS":
      return updateEventTypes("add", state, action.events);

    case "REMOVE_EVENT_LISTENERS":
      return updateEventTypes("remove", state, action.events);

    default:
      return state;
  }
}

function updateEventTypes(
  addOrRemove: string,
  currentEvents: EventListenerBreakpoints,
  events: EventListenerBreakpoints
): EventListenerBreakpoints {
  let newEventListeners;

  if (addOrRemove === "add") {
    newEventListeners = uniq([...currentEvents, ...events]);
  } else {
    newEventListeners = currentEvents.filter(event => !events.includes(event));
  }

  asyncStore.eventListenerBreakpoints = newEventListeners;
  return newEventListeners;
}

export function getActiveEventListeners(state: OuterState) {
  return state.eventListenerBreakpoints;
}

export default update;
