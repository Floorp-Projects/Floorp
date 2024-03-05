/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import flags from "devtools/shared/flags";
import { prefs } from "../../../utils/prefs";

const ignoreList = [
  "ADD_BREAKPOINT_POSITIONS",
  "SET_SYMBOLS",
  "OUT_OF_SCOPE_LOCATIONS",
  "MAP_SCOPES",
  "MAP_FRAMES",
  "ADD_SCOPES",
  "IN_SCOPE_LINES",
  "REMOVE_BREAKPOINT",
  "NODE_PROPERTIES_LOADED",
  "SET_FOCUSED_SOURCE_ITEM",
  "NODE_EXPAND",
  "IN_SCOPE_LINES",
  "SET_PREVIEW",
];

function cloneAction(action) {
  action = action || {};
  action = { ...action };

  // ADD_TAB, ...
  if (action.source?.text) {
    const source = { ...action.source, text: "" };
    action.source = source;
  }

  if (action.sources) {
    const sources = action.sources.slice(0, 20).map(source => {
      const url = !source.url || source.url.includes("data:") ? "" : source.url;
      return { ...source, url };
    });
    action.sources = sources;
  }

  // LOAD_SOURCE_TEXT
  if (action.text) {
    action.text = "";
  }

  if (action.value?.text) {
    const value = { ...action.value, text: "" };
    action.value = value;
  }

  return action;
}

function formatPause(pause) {
  return {
    ...pause,
    pauseInfo: { why: pause.why },
    scopes: [],
    loadedObjects: [],
  };
}

function serializeAction(action) {
  try {
    action = cloneAction(action);
    if (ignoreList.includes(action.type)) {
      action = {};
    }

    if (action.type === "PAUSED") {
      action = formatPause(action);
    }

    const serializer = function (key, value) {
      // Serialize Object/LongString fronts
      if (value?.getGrip) {
        return value.getGrip();
      }
      return value;
    };

    // dump(`> ${action.type}...\n ${JSON.stringify(action, serializer)}\n`);
    return JSON.stringify(action, serializer);
  } catch (e) {
    console.error(e);
    return "";
  }
}

/**
 * A middleware that logs all actions coming through the system
 * to the console.
 */
export function log() {
  return next => action => {
    const asyncMsg = !action.status ? "" : `[${action.status}]`;

    if (prefs.logActions) {
      if (flags.testing) {
        dump(
          `[ACTION] ${action.type} ${asyncMsg} - ${serializeAction(action)}\n`
        );
      } else {
        console.log(action, asyncMsg);
      }
    }

    next(action);
  };
}
