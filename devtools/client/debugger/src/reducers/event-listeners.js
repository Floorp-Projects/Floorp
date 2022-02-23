/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../utils/prefs";

export function initialEventListenerState() {
  return {
    active: [],
    categories: [],
    expanded: [],
    logEventBreakpoints: prefs.logEventBreakpoints,
  };
}

function update(state = initialEventListenerState(), action) {
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

export default update;
