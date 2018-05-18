/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 19);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 1 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* unused harmony export MAIN_MESSAGE_TYPE */
/* unused harmony export CONTENT_MESSAGE_TYPE */
/* unused harmony export PRELOAD_MESSAGE_TYPE */
/* unused harmony export UI_CODE */
/* unused harmony export BACKGROUND_PROCESS */
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "b", function() { return actionCreators; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "d", function() { return actionUtils; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var MAIN_MESSAGE_TYPE = "ActivityStream:Main";
var CONTENT_MESSAGE_TYPE = "ActivityStream:Content";
var PRELOAD_MESSAGE_TYPE = "ActivityStream:PreloadedBrowser";
var UI_CODE = 1;
var BACKGROUND_PROCESS = 2;

/**
 * globalImportContext - Are we in UI code (i.e. react, a dom) or some kind of background process?
 *                       Use this in action creators if you need different logic
 *                       for ui/background processes.
 */

const globalImportContext = typeof Window === "undefined" ? BACKGROUND_PROCESS : UI_CODE;
/* unused harmony export globalImportContext */

// Export for tests

// Create an object that avoids accidental differing key/value pairs:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
const actionTypes = {};
/* harmony export (immutable) */ __webpack_exports__["c"] = actionTypes;


for (const type of ["ARCHIVE_FROM_POCKET", "AS_ROUTER_TELEMETRY_USER_EVENT", "BLOCK_URL", "BOOKMARK_URL", "COPY_DOWNLOAD_LINK", "DELETE_BOOKMARK_BY_ID", "DELETE_FROM_POCKET", "DELETE_HISTORY_URL", "DIALOG_CANCEL", "DIALOG_OPEN", "DISABLE_ONBOARDING", "DOWNLOAD_CHANGED", "INIT", "MIGRATION_CANCEL", "MIGRATION_COMPLETED", "MIGRATION_START", "NEW_TAB_INIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_REHYDRATED", "NEW_TAB_STATE_REQUEST", "NEW_TAB_UNLOAD", "OPEN_DOWNLOAD_FILE", "OPEN_LINK", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "OPEN_WEBEXT_SETTINGS", "PAGE_PRERENDERED", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINKS_CHANGED", "PLACES_LINK_BLOCKED", "PLACES_LINK_DELETED", "PLACES_SAVED_TO_POCKET", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "PREVIEW_REQUEST", "PREVIEW_REQUEST_CANCEL", "PREVIEW_RESPONSE", "REMOVE_DOWNLOAD_FILE", "RICH_ICON_MISSING", "SAVE_SESSION_PERF_DATA", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SECTION_DEREGISTER", "SECTION_DISABLE", "SECTION_ENABLE", "SECTION_MOVE", "SECTION_OPTIONS_CHANGED", "SECTION_REGISTER", "SECTION_UPDATE", "SECTION_UPDATE_CARD", "SETTINGS_CLOSE", "SETTINGS_OPEN", "SET_PREF", "SHOW_DOWNLOAD_FILE", "SHOW_FIREFOX_ACCOUNTS", "SKIPPED_SIGNIN", "SNIPPETS_BLOCKLIST_CLEARED", "SNIPPETS_BLOCKLIST_UPDATED", "SNIPPETS_DATA", "SNIPPETS_RESET", "SNIPPET_BLOCKED", "SUBMIT_EMAIL", "SYSTEM_TICK", "TELEMETRY_IMPRESSION_STATS", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "THEME_UPDATE", "TOP_SITES_CANCEL_EDIT", "TOP_SITES_EDIT", "TOP_SITES_INSERT", "TOP_SITES_PIN", "TOP_SITES_PREFS_UPDATED", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "TOTAL_BOOKMARKS_REQUEST", "TOTAL_BOOKMARKS_RESPONSE", "UNINIT", "UPDATE_SECTION_PREFS", "WEBEXT_CLICK", "WEBEXT_DISMISS"]) {
  actionTypes[type] = type;
}

// These are acceptable actions for AS Router messages to have. They can show up
// as call-to-action buttons in snippets, onboarding tour, etc.
const ASRouterActions = {};
/* harmony export (immutable) */ __webpack_exports__["a"] = ASRouterActions;


for (const type of ["OPEN_PRIVATE_BROWSER_WINDOW", "OPEN_URL", "OPEN_ABOUT_PAGE"]) {
  ASRouterActions[type] = type;
}

// Helper function for creating routed actions between content and main
// Not intended to be used by consumers
function _RouteMessage(action, options) {
  const meta = action.meta ? Object.assign({}, action.meta) : {};
  if (!options || !options.from || !options.to) {
    throw new Error("Routed Messages must have options as the second parameter, and must at least include a .from and .to property.");
  }
  // For each of these fields, if they are passed as an option,
  // add them to the action. If they are not defined, remove them.
  ["from", "to", "toTarget", "fromTarget", "skipMain", "skipLocal"].forEach(o => {
    if (typeof options[o] !== "undefined") {
      meta[o] = options[o];
    } else if (meta[o]) {
      delete meta[o];
    }
  });
  return Object.assign({}, action, { meta });
}

/**
 * AlsoToMain - Creates a message that will be dispatched locally and also sent to the Main process.
 *
 * @param  {object} action Any redux action (required)
 * @param  {object} options
 * @param  {bool}   skipLocal Used by OnlyToMain to skip the main reducer
 * @param  {string} fromTarget The id of the content port from which the action originated. (optional)
 * @return {object} An action with added .meta properties
 */
function AlsoToMain(action, fromTarget, skipLocal) {
  return _RouteMessage(action, {
    from: CONTENT_MESSAGE_TYPE,
    to: MAIN_MESSAGE_TYPE,
    fromTarget,
    skipLocal
  });
}

/**
 * OnlyToMain - Creates a message that will be sent to the Main process and skip the local reducer.
 *
 * @param  {object} action Any redux action (required)
 * @param  {object} options
 * @param  {string} fromTarget The id of the content port from which the action originated. (optional)
 * @return {object} An action with added .meta properties
 */
function OnlyToMain(action, fromTarget) {
  return AlsoToMain(action, fromTarget, true);
}

/**
 * BroadcastToContent - Creates a message that will be dispatched to main and sent to ALL content processes.
 *
 * @param  {object} action Any redux action (required)
 * @return {object} An action with added .meta properties
 */
function BroadcastToContent(action) {
  return _RouteMessage(action, {
    from: MAIN_MESSAGE_TYPE,
    to: CONTENT_MESSAGE_TYPE
  });
}

/**
 * AlsoToOneContent - Creates a message that will be will be dispatched to the main store
 *                    and also sent to a particular Content process.
 *
 * @param  {object} action Any redux action (required)
 * @param  {string} target The id of a content port
 * @param  {bool} skipMain Used by OnlyToOneContent to skip the main process
 * @return {object} An action with added .meta properties
 */
function AlsoToOneContent(action, target, skipMain) {
  if (!target) {
    throw new Error("You must provide a target ID as the second parameter of AlsoToOneContent. If you want to send to all content processes, use BroadcastToContent");
  }
  return _RouteMessage(action, {
    from: MAIN_MESSAGE_TYPE,
    to: CONTENT_MESSAGE_TYPE,
    toTarget: target,
    skipMain
  });
}

/**
 * OnlyToOneContent - Creates a message that will be sent to a particular Content process
 *                    and skip the main reducer.
 *
 * @param  {object} action Any redux action (required)
 * @param  {string} target The id of a content port
 * @return {object} An action with added .meta properties
 */
function OnlyToOneContent(action, target) {
  return AlsoToOneContent(action, target, true);
}

/**
 * AlsoToPreloaded - Creates a message that dispatched to the main reducer and also sent to the preloaded tab.
 *
 * @param  {object} action Any redux action (required)
 * @return {object} An action with added .meta properties
 */
function AlsoToPreloaded(action) {
  return _RouteMessage(action, {
    from: MAIN_MESSAGE_TYPE,
    to: PRELOAD_MESSAGE_TYPE
  });
}

/**
 * UserEvent - A telemetry ping indicating a user action. This should only
 *                   be sent from the UI during a user session.
 *
 * @param  {object} data Fields to include in the ping (source, etc.)
 * @return {object} An AlsoToMain action
 */
function UserEvent(data) {
  return AlsoToMain({
    type: actionTypes.TELEMETRY_USER_EVENT,
    data
  });
}

/**
 * ASRouterUserEvent - A telemetry ping indicating a user action from AS router. This should only
 *                     be sent from the UI during a user session.
 *
 * @param  {object} data Fields to include in the ping (source, etc.)
 * @return {object} An AlsoToMain action
 */
function ASRouterUserEvent(data) {
  return AlsoToMain({
    type: actionTypes.AS_ROUTER_TELEMETRY_USER_EVENT,
    data
  });
}

/**
 * UndesiredEvent - A telemetry ping indicating an undesired state.
 *
 * @param  {object} data Fields to include in the ping (value, etc.)
 * @param  {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a AlsoToMain action.
 */
function UndesiredEvent(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.TELEMETRY_UNDESIRED_EVENT,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

/**
 * PerfEvent - A telemetry ping indicating a performance-related event.
 *
 * @param  {object} data Fields to include in the ping (value, etc.)
 * @param  {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a AlsoToMain action.
 */
function PerfEvent(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.TELEMETRY_PERFORMANCE_EVENT,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

/**
 * ImpressionStats - A telemetry ping indicating an impression stats.
 *
 * @param  {object} data Fields to include in the ping
 * @param  {int} importContext (For testing) Override the import context for testing.
 * #return {object} An action. For UI code, a AlsoToMain action.
 */
function ImpressionStats(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.TELEMETRY_IMPRESSION_STATS,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function SetPref(name, value, importContext = globalImportContext) {
  const action = { type: actionTypes.SET_PREF, data: { name, value } };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function WebExtEvent(type, data, importContext = globalImportContext) {
  if (!data || !data.source) {
    throw new Error("WebExtEvent actions should include a property \"source\", the id of the webextension that should receive the event.");
  }
  const action = { type, data };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

var actionCreators = {
  BroadcastToContent,
  UserEvent,
  ASRouterUserEvent,
  UndesiredEvent,
  PerfEvent,
  ImpressionStats,
  AlsoToOneContent,
  OnlyToOneContent,
  AlsoToMain,
  OnlyToMain,
  AlsoToPreloaded,
  SetPref,
  WebExtEvent
};

// These are helpers to test for certain kinds of actions

var actionUtils = {
  isSendToMain(action) {
    if (!action.meta) {
      return false;
    }
    return action.meta.to === MAIN_MESSAGE_TYPE && action.meta.from === CONTENT_MESSAGE_TYPE;
  },
  isBroadcastToContent(action) {
    if (!action.meta) {
      return false;
    }
    if (action.meta.to === CONTENT_MESSAGE_TYPE && !action.meta.toTarget) {
      return true;
    }
    return false;
  },
  isSendToOneContent(action) {
    if (!action.meta) {
      return false;
    }
    if (action.meta.to === CONTENT_MESSAGE_TYPE && action.meta.toTarget) {
      return true;
    }
    return false;
  },
  isSendToPreloaded(action) {
    if (!action.meta) {
      return false;
    }
    return action.meta.to === PRELOAD_MESSAGE_TYPE && action.meta.from === MAIN_MESSAGE_TYPE;
  },
  isFromMain(action) {
    if (!action.meta) {
      return false;
    }
    return action.meta.from === MAIN_MESSAGE_TYPE && action.meta.to === CONTENT_MESSAGE_TYPE;
  },
  getPortIdOfSender(action) {
    return action.meta && action.meta.fromTarget || null;
  },
  _RouteMessage
};

/***/ }),
/* 2 */
/***/ (function(module, exports) {

module.exports = ReactIntl;

/***/ }),
/* 3 */
/***/ (function(module, exports) {

var g;

// This works in non-strict mode
g = (function() {
	return this;
})();

try {
	// This works if eval is allowed (see CSP)
	g = g || Function("return this")() || (1,eval)("this");
} catch(e) {
	// This works if the window reference is available
	if(typeof window === "object")
		g = window;
}

// g can still be undefined, but nothing to do about it...
// We return undefined, instead of nothing here, so it's
// easier to handle this case. if(!global) { ...}

module.exports = g;


/***/ }),
/* 4 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 5 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
const TOP_SITES_SOURCE = "TOP_SITES";
/* harmony export (immutable) */ __webpack_exports__["d"] = TOP_SITES_SOURCE;

const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];
/* harmony export (immutable) */ __webpack_exports__["c"] = TOP_SITES_CONTEXT_MENU_OPTIONS;

// minimum size necessary to show a rich icon instead of a screenshot
const MIN_RICH_FAVICON_SIZE = 96;
/* harmony export (immutable) */ __webpack_exports__["b"] = MIN_RICH_FAVICON_SIZE;

// minimum size necessary to show any icon in the top left corner with a screenshot
const MIN_CORNER_FAVICON_SIZE = 16;
/* harmony export (immutable) */ __webpack_exports__["a"] = MIN_CORNER_FAVICON_SIZE;


/***/ }),
/* 6 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./system-addon/common/Actions.jsm
var Actions = __webpack_require__(1);

// CONCATENATED MODULE: ./system-addon/common/Dedupe.jsm
class Dedupe {
  constructor(createKey) {
    this.createKey = createKey || this.defaultCreateKey;
  }

  defaultCreateKey(item) {
    return item;
  }

  /**
   * Dedupe any number of grouped elements favoring those from earlier groups.
   *
   * @param {Array} groups Contains an arbitrary number of arrays of elements.
   * @returns {Array} A matching array of each provided group deduped.
   */
  group(...groups) {
    const globalKeys = new Set();
    const result = [];
    for (const values of groups) {
      const valueMap = new Map();
      for (const value of values) {
        const key = this.createKey(value);
        if (!globalKeys.has(key) && !valueMap.has(key)) {
          valueMap.set(key, value);
        }
      }
      result.push(valueMap);
      valueMap.forEach((value, key) => globalKeys.add(key));
    }
    return result.map(m => Array.from(m.values()));
  }
}
// CONCATENATED MODULE: ./system-addon/common/Reducers.jsm
/* unused harmony export insertPinned */
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "b", function() { return reducers; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */




const TOP_SITES_DEFAULT_ROWS = 1;
/* unused harmony export TOP_SITES_DEFAULT_ROWS */

const TOP_SITES_MAX_SITES_PER_ROW = 8;
/* harmony export (immutable) */ __webpack_exports__["a"] = TOP_SITES_MAX_SITES_PER_ROW;



const dedupe = new Dedupe(site => site && site.url);

const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    // The version of the system-addon
    version: null
  },
  Snippets: { initialized: false },
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: [],
    // Used in content only to dispatch action to TopSiteForm.
    editForm: null
  },
  Prefs: {
    initialized: false,
    values: {}
  },
  Theme: { className: "" },
  Dialog: {
    visible: false,
    data: {}
  },
  Sections: []
};
/* unused harmony export INITIAL_STATE */



function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case Actions["c" /* actionTypes */].INIT:
      return Object.assign({}, prevState, action.data || {}, { initialized: true });
    default:
      return prevState;
  }
}

/**
 * insertPinned - Inserts pinned links in their specified slots
 *
 * @param {array} a list of links
 * @param {array} a list of pinned links
 * @return {array} resulting list of links with pinned links inserted
 */
function insertPinned(links, pinned) {
  // Remove any pinned links
  const pinnedUrls = pinned.map(link => link && link.url);
  let newLinks = links.filter(link => link ? !pinnedUrls.includes(link.url) : false);
  newLinks = newLinks.map(link => {
    if (link && link.isPinned) {
      delete link.isPinned;
      delete link.pinIndex;
    }
    return link;
  });

  // Then insert them in their specified location
  pinned.forEach((val, index) => {
    if (!val) {
      return;
    }
    let link = Object.assign({}, val, { isPinned: true, pinIndex: index });
    if (index > newLinks.length) {
      newLinks[index] = link;
    } else {
      newLinks.splice(index, 0, link);
    }
  });

  return newLinks;
}


function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  let hasMatch;
  let newRows;
  switch (action.type) {
    case Actions["c" /* actionTypes */].TOP_SITES_UPDATED:
      if (!action.data || !action.data.links) {
        return prevState;
      }
      return Object.assign({}, prevState, { initialized: true, rows: action.data.links }, action.data.pref ? { pref: action.data.pref } : {});
    case Actions["c" /* actionTypes */].TOP_SITES_PREFS_UPDATED:
      return Object.assign({}, prevState, { pref: action.data.pref });
    case Actions["c" /* actionTypes */].TOP_SITES_EDIT:
      return Object.assign({}, prevState, {
        editForm: {
          index: action.data.index,
          previewResponse: null
        }
      });
    case Actions["c" /* actionTypes */].TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, { editForm: null });
    case Actions["c" /* actionTypes */].PREVIEW_RESPONSE:
      if (!prevState.editForm || action.data.url !== prevState.editForm.previewUrl) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: action.data.preview,
          previewUrl: action.data.url
        }
      });
    case Actions["c" /* actionTypes */].PREVIEW_REQUEST:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null,
          previewUrl: action.data.url
        }
      });
    case Actions["c" /* actionTypes */].PREVIEW_REQUEST_CANCEL:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null
        }
      });
    case Actions["c" /* actionTypes */].SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, { rows: newRows }) : prevState;
    case Actions["c" /* actionTypes */].PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
          return Object.assign({}, site, { bookmarkGuid, bookmarkTitle, bookmarkDateCreated: dateAdded });
        }
        return site;
      });
      return Object.assign({}, prevState, { rows: newRows });
    case Actions["c" /* actionTypes */].PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const newSite = Object.assign({}, site);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;
          return newSite;
        }
        return site;
      });
      return Object.assign({}, prevState, { rows: newRows });
    case Actions["c" /* actionTypes */].PLACES_LINK_DELETED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.filter(site => action.data.url !== site.url);
      return Object.assign({}, prevState, { rows: newRows });
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case Actions["c" /* actionTypes */].DIALOG_OPEN:
      return Object.assign({}, prevState, { visible: true, data: action.data });
    case Actions["c" /* actionTypes */].DIALOG_CANCEL:
      return Object.assign({}, prevState, { visible: false });
    case Actions["c" /* actionTypes */].DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case Actions["c" /* actionTypes */].PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, { initialized: true, values: action.data });
    case Actions["c" /* actionTypes */].PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, { values: newValues });
    default:
      return prevState;
  }
}

function Sections(prevState = INITIAL_STATE.Sections, action) {
  let hasMatch;
  let newState;
  switch (action.type) {
    case Actions["c" /* actionTypes */].SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case Actions["c" /* actionTypes */].SECTION_REGISTER:
      // If section exists in prevState, update it
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          hasMatch = true;
          return Object.assign({}, section, action.data);
        }
        return section;
      });
      // Otherwise, append it
      if (!hasMatch) {
        const initialized = !!(action.data.rows && action.data.rows.length > 0);
        const section = Object.assign({ title: "", rows: [], enabled: false }, action.data, { initialized });
        newState.push(section);
      }
      return newState;
    case Actions["c" /* actionTypes */].SECTION_UPDATE:
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          // If the action is updating rows, we should consider initialized to be true.
          // This can be overridden if initialized is defined in the action.data
          const initialized = action.data.rows ? { initialized: true } : {};

          // Make sure pinned cards stay at their current position when rows are updated.
          // Disabling a section (SECTION_UPDATE with empty rows) does not retain pinned cards.
          if (action.data.rows && action.data.rows.length > 0 && section.rows.find(card => card.pinned)) {
            const rows = Array.from(action.data.rows);
            section.rows.forEach((card, index) => {
              if (card.pinned) {
                rows.splice(index, 0, card);
              }
            });
            return Object.assign({}, section, initialized, Object.assign({}, action.data, { rows }));
          }

          return Object.assign({}, section, initialized, action.data);
        }
        return section;
      });

      if (!action.data.dedupeConfigurations) {
        return newState;
      }

      action.data.dedupeConfigurations.forEach(dedupeConf => {
        newState = newState.map(section => {
          if (section.id === dedupeConf.id) {
            const dedupedRows = dedupeConf.dedupeFrom.reduce((rows, dedupeSectionId) => {
              const dedupeSection = newState.find(s => s.id === dedupeSectionId);
              const [, newRows] = dedupe.group(dedupeSection.rows, rows);
              return newRows;
            }, section.rows);

            return Object.assign({}, section, { rows: dedupedRows });
          }

          return section;
        });
      });

      return newState;
    case Actions["c" /* actionTypes */].SECTION_UPDATE_CARD:
      return prevState.map(section => {
        if (section && section.id === action.data.id && section.rows) {
          const newRows = section.rows.map(card => {
            if (card.url === action.data.url) {
              return Object.assign({}, card, action.data.options);
            }
            return card;
          });
          return Object.assign({}, section, { rows: newRows });
        }
        return section;
      });
    case Actions["c" /* actionTypes */].PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the item within the rows that is attempted to be bookmarked
          if (item.url === action.data.url) {
            const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
            return Object.assign({}, item, {
              bookmarkGuid,
              bookmarkTitle,
              bookmarkDateCreated: dateAdded,
              type: "bookmark"
            });
          }
          return item;
        })
      }));
    case Actions["c" /* actionTypes */].PLACES_SAVED_TO_POCKET:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          if (item.url === action.data.url) {
            return Object.assign({}, item, {
              open_url: action.data.open_url,
              pocket_id: action.data.pocket_id,
              title: action.data.title,
              type: "pocket"
            });
          }
          return item;
        })
      }));
    case Actions["c" /* actionTypes */].PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the bookmark within the rows that is attempted to be removed
          if (item.url === action.data.url) {
            const newSite = Object.assign({}, item);
            delete newSite.bookmarkGuid;
            delete newSite.bookmarkTitle;
            delete newSite.bookmarkDateCreated;
            if (!newSite.type || newSite.type === "bookmark") {
              newSite.type = "history";
            }
            return newSite;
          }
          return item;
        })
      }));
    case Actions["c" /* actionTypes */].PLACES_LINK_DELETED:
    case Actions["c" /* actionTypes */].PLACES_LINK_BLOCKED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.url !== action.data.url) }));
    case Actions["c" /* actionTypes */].DELETE_FROM_POCKET:
    case Actions["c" /* actionTypes */].ARCHIVE_FROM_POCKET:
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.pocket_id !== action.data.pocket_id) }));
    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case Actions["c" /* actionTypes */].SNIPPETS_DATA:
      return Object.assign({}, prevState, { initialized: true }, action.data);
    case Actions["c" /* actionTypes */].SNIPPET_BLOCKED:
      return Object.assign({}, prevState, { blockList: prevState.blockList.concat(action.data) });
    case Actions["c" /* actionTypes */].SNIPPETS_BLOCKLIST_CLEARED:
      return Object.assign({}, prevState, { blockList: [] });
    case Actions["c" /* actionTypes */].SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

function Theme(prevState = INITIAL_STATE.Theme, action) {
  switch (action.type) {
    case Actions["c" /* actionTypes */].THEME_UPDATE:
      return Object.assign({}, prevState, action.data);
    default:
      return prevState;
  }
}

var reducers = { TopSites, App, Snippets, Prefs, Dialog, Sections, Theme };

/***/ }),
/* 7 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (immutable) */ __webpack_exports__["b"] = initASRouter;
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_content_src_lib_init_store__ = __webpack_require__(8);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2__components_ImpressionsWrapper_ImpressionsWrapper__ = __webpack_require__(22);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3__templates_OnboardingMessage_OnboardingMessage__ = __webpack_require__(23);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_4_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_dom__ = __webpack_require__(9);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_dom___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_5_react_dom__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6__templates_SimpleSnippet_SimpleSnippet__ = __webpack_require__(24);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };









const INCOMING_MESSAGE_NAME = "ASRouter:parent-to-child";
const OUTGOING_MESSAGE_NAME = "ASRouter:child-to-parent";

const ASRouterUtils = {
  addListener(listener) {
    global.addMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  removeListener(listener) {
    global.removeMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  sendMessage(action) {
    global.sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  },
  blockById(id) {
    ASRouterUtils.sendMessage({ type: "BLOCK_MESSAGE_BY_ID", data: { id } });
  },
  blockBundle(bundle) {
    ASRouterUtils.sendMessage({ type: "BLOCK_BUNDLE", data: { bundle } });
  },
  executeAction({ button_action, button_action_params }) {
    if (button_action in __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["a" /* ASRouterActions */]) {
      ASRouterUtils.sendMessage({ type: button_action, data: { button_action_params } });
    }
  },
  unblockById(id) {
    ASRouterUtils.sendMessage({ type: "UNBLOCK_MESSAGE_BY_ID", data: { id } });
  },
  unblockBundle(bundle) {
    ASRouterUtils.sendMessage({ type: "UNBLOCK_BUNDLE", data: { bundle } });
  },
  getNextMessage() {
    ASRouterUtils.sendMessage({ type: "GET_NEXT_MESSAGE" });
  },
  overrideMessage(id) {
    ASRouterUtils.sendMessage({ type: "OVERRIDE_MESSAGE", data: { id } });
  },
  sendTelemetry(ping) {
    const payload = __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].ASRouterUserEvent(ping);
    global.sendAsyncMessage(__WEBPACK_IMPORTED_MODULE_1_content_src_lib_init_store__["a" /* OUTGOING_MESSAGE_NAME */], payload);
  }
};
/* harmony export (immutable) */ __webpack_exports__["a"] = ASRouterUtils;


// Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />
function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id);
}

class ASRouterUISurface extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.sendImpression = this.sendImpression.bind(this);
    this.sendUserActionTelemetry = this.sendUserActionTelemetry.bind(this);
    this.state = { message: {}, bundle: {} };
  }

  sendUserActionTelemetry(extraProps = {}) {
    const { message, bundle } = this.state;
    if (!message && !extraProps.message_id) {
      throw new Error(`You must provide a message_id for bundled messages`);
    }
    const eventType = `${message.provider || bundle.provider}_user_event`;

    ASRouterUtils.sendTelemetry(Object.assign({
      message_id: message.id || extraProps.message_id,
      source: this.props.id,
      action: eventType
    }, extraProps));
  }

  sendImpression() {
    this.sendUserActionTelemetry({ event: "IMPRESSION" });
  }

  onBlockById(id) {
    return () => ASRouterUtils.blockById(id);
  }

  clearBundle(bundle) {
    return () => ASRouterUtils.blockBundle(bundle);
  }

  onMessageFromParent({ data: action }) {
    switch (action.type) {
      case "SET_MESSAGE":
        this.setState({ message: action.data });
        break;
      case "SET_BUNDLED_MESSAGES":
        this.setState({ bundle: action.data });
        break;
      case "CLEAR_MESSAGE":
        this.setState({ message: {}, bundle: {} });
        break;
    }
  }

  componentWillMount() {
    ASRouterUtils.addListener(this.onMessageFromParent);
    ASRouterUtils.sendMessage({ type: "CONNECT_UI_REQUEST" });
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  renderSnippets() {
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      __WEBPACK_IMPORTED_MODULE_2__components_ImpressionsWrapper_ImpressionsWrapper__["a" /* ImpressionsWrapper */],
      {
        message: this.state.message,
        sendImpression: this.sendImpression,
        shouldSendImpressionOnUpdate: shouldSendImpressionOnUpdate
        // This helps with testing
        , document: this.props.document },
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_6__templates_SimpleSnippet_SimpleSnippet__["a" /* SimpleSnippet */], _extends({}, this.state.message, {
        UISurface: "NEWTAB_FOOTER_BAR",
        getNextMessage: ASRouterUtils.getNextMessage,
        onBlock: this.onBlockById(this.state.message.id),
        sendUserActionTelemetry: this.sendUserActionTelemetry }))
    );
  }

  renderOnboarding() {
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_3__templates_OnboardingMessage_OnboardingMessage__["a" /* OnboardingMessage */], _extends({}, this.state.bundle, {
      UISurface: "NEWTAB_OVERLAY",
      onAction: ASRouterUtils.executeAction,
      onDoneButton: this.clearBundle(this.state.bundle.bundle),
      getNextMessage: ASRouterUtils.getNextMessage,
      sendUserActionTelemetry: this.sendUserActionTelemetry }));
  }

  render() {
    const { message, bundle } = this.state;
    if (!message.id && !bundle.template) {
      return null;
    }
    if (bundle.template === "onboarding") {
      return this.renderOnboarding();
    }
    return this.renderSnippets();
  }
}
/* unused harmony export ASRouterUISurface */


ASRouterUISurface.defaultProps = { document: global.document };

function initASRouter() {
  __WEBPACK_IMPORTED_MODULE_5_react_dom___default.a.render(__WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(ASRouterUISurface, null), document.getElementById("snippets-container"));
}
/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 8 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (immutable) */ __webpack_exports__["b"] = initStore;
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_redux__ = __webpack_require__(21);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_redux__);
/* eslint-env mozilla/frame-script */




const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
/* unused harmony export MERGE_STORE_ACTION */

const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
/* harmony export (immutable) */ __webpack_exports__["a"] = OUTGOING_MESSAGE_NAME;

const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
/* unused harmony export INCOMING_MESSAGE_NAME */

const EARLY_QUEUED_ACTIONS = [__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_SESSION_PERF_DATA, __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].PAGE_PRERENDERED];
/* unused harmony export EARLY_QUEUED_ACTIONS */


/**
 * A higher-order function which returns a reducer that, on MERGE_STORE action,
 * will return the action.data object merged into the previous state.
 *
 * For all other actions, it merely calls mainReducer.
 *
 * Because we want this to merge the entire state object, it's written as a
 * higher order function which takes the main reducer (itself often a call to
 * combineReducers) as a parameter.
 *
 * @param  {function} mainReducer reducer to call if action != MERGE_STORE_ACTION
 * @return {function}             a reducer that, on MERGE_STORE_ACTION action,
 *                                will return the action.data object merged
 *                                into the previous state, and the result
 *                                of calling mainReducer otherwise.
 */
function mergeStateReducer(mainReducer) {
  return (prevState, action) => {
    if (action.type === MERGE_STORE_ACTION) {
      return Object.assign({}, prevState, action.data);
    }

    return mainReducer(prevState, action);
  };
}

/**
 * messageMiddleware - Middleware that looks for SentToMain type actions, and sends them if necessary
 */
const messageMiddleware = store => next => action => {
  const skipLocal = action.meta && action.meta.skipLocal;
  if (__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["d" /* actionUtils */].isSendToMain(action)) {
    sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  }
  if (!skipLocal) {
    next(action);
  }
};

const rehydrationMiddleware = store => next => action => {
  if (store._didRehydrate) {
    return next(action);
  }

  const isMergeStoreAction = action.type === MERGE_STORE_ACTION;
  const isRehydrationRequest = action.type === __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].NEW_TAB_STATE_REQUEST;

  if (isRehydrationRequest) {
    store._didRequestInitialState = true;
    return next(action);
  }

  if (isMergeStoreAction) {
    store._didRehydrate = true;
    return next(action);
  }

  // If init happened after our request was made, we need to re-request
  if (store._didRequestInitialState && action.type === __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].INIT) {
    return next(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].NEW_TAB_STATE_REQUEST }));
  }

  if (__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["d" /* actionUtils */].isBroadcastToContent(action) || __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["d" /* actionUtils */].isSendToOneContent(action) || __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["d" /* actionUtils */].isSendToPreloaded(action)) {
    // Note that actions received before didRehydrate will not be dispatched
    // because this could negatively affect preloading and the the state
    // will be replaced by rehydration anyway.
    return null;
  }

  return next(action);
};
/* unused harmony export rehydrationMiddleware */


/**
 * This middleware queues up all the EARLY_QUEUED_ACTIONS until it receives
 * the first action from main. This is useful for those actions for main which
 * require higher reliability, i.e. the action will not be lost in the case
 * that it gets sent before the main is ready to receive it. Conversely, any
 * actions allowed early are accepted to be ignorable or re-sendable.
 */
const queueEarlyMessageMiddleware = store => next => action => {
  if (store._receivedFromMain) {
    next(action);
  } else if (__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["d" /* actionUtils */].isFromMain(action)) {
    next(action);
    store._receivedFromMain = true;
    // Sending out all the early actions as main is ready now
    if (store._earlyActionQueue) {
      store._earlyActionQueue.forEach(next);
      store._earlyActionQueue = [];
    }
  } else if (EARLY_QUEUED_ACTIONS.includes(action.type)) {
    store._earlyActionQueue = store._earlyActionQueue || [];
    store._earlyActionQueue.push(action);
  } else {
    // Let any other type of action go through
    next(action);
  }
};
/* unused harmony export queueEarlyMessageMiddleware */


/**
 * initStore - Create a store and listen for incoming actions
 *
 * @param  {object} reducers An object containing Redux reducers
 * @param  {object} intialState (optional) The initial state of the store, if desired
 * @return {object}          A redux store
 */
function initStore(reducers, initialState) {
  const store = Object(__WEBPACK_IMPORTED_MODULE_1_redux__["createStore"])(mergeStateReducer(Object(__WEBPACK_IMPORTED_MODULE_1_redux__["combineReducers"])(reducers)), initialState, global.addMessageListener && Object(__WEBPACK_IMPORTED_MODULE_1_redux__["applyMiddleware"])(rehydrationMiddleware, queueEarlyMessageMiddleware, messageMiddleware));

  store._didRehydrate = false;
  store._didRequestInitialState = false;

  if (global.addMessageListener) {
    global.addMessageListener(INCOMING_MESSAGE_NAME, msg => {
      try {
        store.dispatch(msg.data);
      } catch (ex) {
        console.error("Content msg:", msg, "Dispatch error: ", ex); // eslint-disable-line no-console
        dump(`Content msg: ${JSON.stringify(msg)}\nDispatch error: ${ex}\n${ex.stack}`);
      }
    });
  }

  return store;
}
/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 9 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 10 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react__);



class ErrorBoundaryFallback extends __WEBPACK_IMPORTED_MODULE_1_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.windowObj = this.props.windowObj || window;
    this.onClick = this.onClick.bind(this);
  }

  /**
   * Since we only get here if part of the page has crashed, do a
   * forced reload to give us the best chance at recovering.
   */
  onClick() {
    this.windowObj.location.reload(true);
  }

  render() {
    const defaultClass = "as-error-fallback";
    let className;
    if ("className" in this.props) {
      className = `${this.props.className} ${defaultClass}`;
    } else {
      className = defaultClass;
    }

    // href="#" to force normal link styling stuff (eg cursor on hover)
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "div",
      { className: className },
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "div",
        null,
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], {
          defaultMessage: "Oops, something went wrong loading this content.",
          id: "error_fallback_default_info" })
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "span",
        null,
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "a",
          { href: "#", className: "reload-button", onClick: this.onClick },
          __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], {
            defaultMessage: "Refresh page to try again.",
            id: "error_fallback_default_refresh_suggestion" })
        )
      )
    );
  }
}
/* unused harmony export ErrorBoundaryFallback */

ErrorBoundaryFallback.defaultProps = { className: "as-error-fallback" };

class ErrorBoundary extends __WEBPACK_IMPORTED_MODULE_1_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = { hasError: false };
  }

  componentDidCatch(error, info) {
    this.setState({ hasError: true });
  }

  render() {
    if (!this.state.hasError) {
      return this.props.children;
    }

    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(this.props.FallbackComponent, { className: this.props.className });
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = ErrorBoundary;


ErrorBoundary.defaultProps = { FallbackComponent: ErrorBoundaryFallback };

/***/ }),
/* 11 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);


const _OpenInPrivateWindow = site => ({
  id: "menu_action_open_private_window",
  icon: "new-window-private",
  action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
    type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_PRIVATE_WINDOW,
    data: { url: site.url, referrer: site.referrer }
  }),
  userEvent: "OPEN_PRIVATE_WINDOW"
});

const GetPlatformString = platform => {
  switch (platform) {
    case "win":
      return "menu_action_show_file_windows";
    case "macosx":
      return "menu_action_show_file_mac_os";
    case "linux":
      return "menu_action_show_file_linux";
    default:
      return "menu_action_show_file_default";
  }
};
/* harmony export (immutable) */ __webpack_exports__["a"] = GetPlatformString;


/**
 * List of functions that return items that can be included as menu options in a
 * LinkMenu. All functions take the site as the first parameter, and optionally
 * the index of the site.
 */
const LinkMenuOptions = {
  Separator: () => ({ type: "separator" }),
  EmptyItem: () => ({ type: "empty" }),
  RemoveBookmark: site => ({
    id: "menu_action_remove_bookmark",
    icon: "bookmark-added",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "menu_action_bookmark",
    icon: "bookmark-hollow",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].BOOKMARK_URL,
      data: { url: site.url, title: site.title, type: site.type }
    }),
    userEvent: "BOOKMARK_ADD"
  }),
  OpenInNewWindow: site => ({
    id: "menu_action_open_new_window",
    icon: "new-window",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_NEW_WINDOW,
      data: {
        referrer: site.referrer,
        typedBonus: site.typedBonus,
        url: site.url
      }
    }),
    userEvent: "OPEN_NEW_WINDOW"
  }),
  BlockUrl: (site, index, eventSource) => ({
    id: "menu_action_dismiss",
    icon: "dismiss",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].BLOCK_URL,
      data: { url: site.url, pocket_id: site.pocket_id }
    }),
    impression: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].ImpressionStats({
      source: eventSource,
      block: 0,
      tiles: [{ id: site.guid, pos: index }]
    }),
    userEvent: "BLOCK"
  }),

  // This is an option for web extentions which will result in remove items from
  // memory and notify the web extenion, rather than using the built-in block list.
  WebExtDismiss: (site, index, eventSource) => ({
    id: "menu_action_webext_dismiss",
    string_id: "menu_action_dismiss",
    icon: "dismiss",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].WebExtEvent(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].WEBEXT_DISMISS, {
      source: eventSource,
      url: site.url,
      action_position: index
    })
  }),
  DeleteUrl: (site, index, eventSource, isEnabled, siteInfo) => ({
    id: "menu_action_delete",
    icon: "delete",
    action: {
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DIALOG_OPEN,
      data: {
        onConfirm: [__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DELETE_HISTORY_URL, data: { url: site.url, pocket_id: site.pocket_id, forceBlock: site.bookmarkGuid } }), __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent(Object.assign({ event: "DELETE", source: eventSource, action_position: index }, siteInfo))],
        eventSource,
        body_string_id: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
        confirm_button_string_id: "menu_action_delete",
        cancel_button_string_id: "topsites_form_cancel_button",
        icon: "modal-delete"
      }
    },
    userEvent: "DIALOG_OPEN"
  }),
  ShowFile: (site, index, eventSource, isEnabled, siteInfo, platform) => ({
    id: GetPlatformString(platform),
    icon: "search",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SHOW_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  OpenFile: site => ({
    id: "menu_action_open_file",
    icon: "open-file",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  CopyDownloadLink: site => ({
    id: "menu_action_copy_download_link",
    icon: "copy",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].COPY_DOWNLOAD_LINK,
      data: { url: site.url }
    })
  }),
  GoToDownloadPage: site => ({
    id: "menu_action_go_to_download_page",
    icon: "download",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_LINK,
      data: { url: site.referrer }
    }),
    disabled: !site.referrer
  }),
  RemoveDownload: site => ({
    id: "menu_action_remove_download",
    icon: "delete",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].REMOVE_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  PinTopSite: (site, index) => ({
    id: "menu_action_pin",
    icon: "pin",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_PIN,
      data: { site: { url: site.url }, index }
    }),
    userEvent: "PIN"
  }),
  UnpinTopSite: site => ({
    id: "menu_action_unpin",
    icon: "unpin",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_UNPIN,
      data: { site: { url: site.url } }
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: (site, index, eventSource) => ({
    id: "menu_action_save_to_pocket",
    icon: "pocket",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_TO_POCKET,
      data: { site: { url: site.url, title: site.title } }
    }),
    impression: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].ImpressionStats({
      source: eventSource,
      pocket: 0,
      tiles: [{ id: site.guid, pos: index }]
    }),
    userEvent: "SAVE_TO_POCKET"
  }),
  DeleteFromPocket: site => ({
    id: "menu_action_delete_pocket",
    icon: "delete",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DELETE_FROM_POCKET,
      data: { pocket_id: site.pocket_id }
    }),
    userEvent: "DELETE_FROM_POCKET"
  }),
  ArchiveFromPocket: site => ({
    id: "menu_action_archive_pocket",
    icon: "check",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].ARCHIVE_FROM_POCKET,
      data: { pocket_id: site.pocket_id }
    }),
    userEvent: "ARCHIVE_FROM_POCKET"
  }),
  EditTopSite: (site, index) => ({
    id: "edit_topsites_button_text",
    icon: "edit",
    action: {
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_EDIT,
      data: { index }
    }
  }),
  CheckBookmark: site => site.bookmarkGuid ? LinkMenuOptions.RemoveBookmark(site) : LinkMenuOptions.AddBookmark(site),
  CheckPinTopSite: (site, index) => site.isPinned ? LinkMenuOptions.UnpinTopSite(site) : LinkMenuOptions.PinTopSite(site, index),
  CheckSavedToPocket: (site, index) => site.pocket_id ? LinkMenuOptions.DeleteFromPocket(site) : LinkMenuOptions.SaveToPocket(site, index),
  CheckBookmarkOrArchive: site => site.pocket_id ? LinkMenuOptions.ArchiveFromPocket(site) : LinkMenuOptions.CheckBookmark(site),
  OpenInPrivateWindow: (site, index, eventSource, isEnabled) => isEnabled ? _OpenInPrivateWindow(site) : LinkMenuOptions.EmptyItem()
};
/* harmony export (immutable) */ __webpack_exports__["b"] = LinkMenuOptions;


/***/ }),
/* 12 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_content_src_components_ContextMenu_ContextMenu__ = __webpack_require__(13);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_content_src_lib_link_menu_options__ = __webpack_require__(11);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_5_react__);







const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];

class _LinkMenu extends __WEBPACK_IMPORTED_MODULE_5_react___default.a.PureComponent {
  getOptions() {
    const { props } = this;
    const { site, index, source, isPrivateBrowsingEnabled, siteInfo, platform } = props;

    // Handle special case of default site
    const propOptions = !site.isDefault ? props.options : DEFAULT_SITE_MENU_OPTIONS;

    const options = propOptions.map(o => __WEBPACK_IMPORTED_MODULE_4_content_src_lib_link_menu_options__["b" /* LinkMenuOptions */][o](site, index, source, isPrivateBrowsingEnabled, siteInfo, platform)).map(option => {
      const { action, impression, id, string_id, type, userEvent } = option;
      if (!type && id) {
        option.label = props.intl.formatMessage({ id: string_id || id });
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            const userEventData = Object.assign({
              event: userEvent,
              source,
              action_position: index
            }, siteInfo);
            props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent(userEventData));
          }
          if (impression && props.shouldSendImpressionStats) {
            props.dispatch(impression);
          }
        };
      }
      return option;
    });

    // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.
    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_5_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_content_src_components_ContextMenu_ContextMenu__["a" /* ContextMenu */], {
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}
/* unused harmony export _LinkMenu */


const getState = state => ({ isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled, platform: state.Prefs.values.platform });
const LinkMenu = Object(__WEBPACK_IMPORTED_MODULE_1_react_redux__["connect"])(getState)(Object(__WEBPACK_IMPORTED_MODULE_3_react_intl__["injectIntl"])(_LinkMenu));
/* harmony export (immutable) */ __webpack_exports__["a"] = LinkMenu;


/***/ }),
/* 13 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react__);


class ContextMenu extends __WEBPACK_IMPORTED_MODULE_0_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.hideContext = this.hideContext.bind(this);
    this.onClick = this.onClick.bind(this);
  }

  hideContext() {
    this.props.onUpdate(false);
  }

  componentDidMount() {
    setTimeout(() => {
      global.addEventListener("click", this.hideContext);
    }, 0);
  }

  componentWillUnmount() {
    global.removeEventListener("click", this.hideContext);
  }

  onClick(event) {
    // Eat all clicks on the context menu so they don't bubble up to window.
    // This prevents the context menu from closing when clicking disabled items
    // or the separators.
    event.stopPropagation();
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement(
      "span",
      { className: "context-menu", onClick: this.onClick },
      __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement(
        "ul",
        { role: "menu", className: "context-menu-list" },
        this.props.options.map((option, i) => option.type === "separator" ? __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement("li", { key: i, className: "separator" }) : option.type !== "empty" && __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement(ContextMenuItem, { key: i, option: option, hideContext: this.hideContext }))
      )
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = ContextMenu;


class ContextMenuItem extends __WEBPACK_IMPORTED_MODULE_0_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  onClick() {
    this.props.hideContext();
    this.props.option.onClick();
  }

  onKeyDown(event) {
    const { option } = this.props;
    switch (event.key) {
      case "Tab":
        // tab goes down in context menu, shift + tab goes up in context menu
        // if we're on the last item, one more tab will close the context menu
        // similarly, if we're on the first item, one more shift + tab will close it
        if (event.shiftKey && option.first || !event.shiftKey && option.last) {
          this.props.hideContext();
        }
        break;
      case "Enter":
        this.props.hideContext();
        option.onClick();
        break;
    }
  }

  render() {
    const { option } = this.props;
    return __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement(
      "li",
      { role: "menuitem", className: "context-menu-item" },
      __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement(
        "a",
        { onClick: this.onClick, onKeyDown: this.onKeyDown, tabIndex: "0", className: option.disabled ? "disabled" : "" },
        option.icon && __WEBPACK_IMPORTED_MODULE_0_react___default.a.createElement("span", { className: `icon icon-spacer icon-${option.icon}` }),
        option.label
      )
    );
  }
}
/* unused harmony export ContextMenuItem */

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 14 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_content_src_components_ErrorBoundary_ErrorBoundary__ = __webpack_require__(10);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_content_src_components_SectionMenu_SectionMenu__ = __webpack_require__(34);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_content_src_lib_section_menu_options__ = __webpack_require__(15);







const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

function getFormattedMessage(message) {
  return typeof message === "string" ? __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
    "span",
    null,
    message
  ) : __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], message);
}

class Disclaimer extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onAcknowledge = this.onAcknowledge.bind(this);
  }

  onAcknowledge() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].SetPref(this.props.disclaimerPref, false));
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: "DISCLAIMER_ACKED", source: this.props.eventSource }));
  }

  render() {
    const { disclaimer } = this.props;
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "div",
      { className: "section-disclaimer" },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "div",
        { className: "section-disclaimer-text" },
        getFormattedMessage(disclaimer.text),
        disclaimer.link && __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "a",
          { href: disclaimer.link.href, target: "_blank", rel: "noopener noreferrer" },
          getFormattedMessage(disclaimer.link.title || disclaimer.link)
        )
      ),
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "button",
        { onClick: this.onAcknowledge },
        getFormattedMessage(disclaimer.button)
      )
    );
  }
}
/* unused harmony export Disclaimer */


const DisclaimerIntl = Object(__WEBPACK_IMPORTED_MODULE_0_react_intl__["injectIntl"])(Disclaimer);
/* unused harmony export DisclaimerIntl */


class _CollapsibleSection extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.onHeaderClick = this.onHeaderClick.bind(this);
    this.onTransitionEnd = this.onTransitionEnd.bind(this);
    this.enableOrDisableAnimation = this.enableOrDisableAnimation.bind(this);
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuButtonMouseEnter = this.onMenuButtonMouseEnter.bind(this);
    this.onMenuButtonMouseLeave = this.onMenuButtonMouseLeave.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.state = { enableAnimation: true, isAnimating: false, menuButtonHover: false, showContextMenu: false };
  }

  componentWillMount() {
    this.props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this.enableOrDisableAnimation);
  }

  componentWillUpdate(nextProps) {
    // Check if we're about to go from expanded to collapsed
    if (!this.props.collapsed && nextProps.collapsed) {
      // This next line forces a layout flush of the section body, which has a
      // max-height style set, so that the upcoming collapse animation can
      // animate from that height to the collapsed height. Without this, the
      // update is coalesced and there's no animation from no-max-height to 0.
      this.sectionBody.scrollHeight; // eslint-disable-line no-unused-expressions
    }
  }

  componentWillUnmount() {
    this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this.enableOrDisableAnimation);
  }

  enableOrDisableAnimation() {
    // Only animate the collapse/expand for visible tabs.
    const visible = this.props.document.visibilityState === VISIBLE;
    if (this.state.enableAnimation !== visible) {
      this.setState({ enableAnimation: visible });
    }
  }

  onBodyMount(node) {
    this.sectionBody = node;
  }

  onHeaderClick() {
    // If this.sectionBody is unset, it means that we're in some sort of error
    // state, probably displaying the error fallback, so we won't be able to
    // compute the height, and we don't want to persist the preference.
    // If props.collapsed is undefined handler shouldn't do anything.
    if (!this.sectionBody || this.props.collapsed === undefined) {
      return;
    }

    // Get the current height of the body so max-height transitions can work
    this.setState({
      isAnimating: true,
      maxHeight: `${this.sectionBody.scrollHeight}px`
    });
    const { action, userEvent } = __WEBPACK_IMPORTED_MODULE_5_content_src_lib_section_menu_options__["a" /* SectionMenuOptions */].CheckCollapsed(this.props);
    this.props.dispatch(action);
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].UserEvent({
      event: userEvent,
      source: this.props.source
    }));
  }

  onTransitionEnd(event) {
    // Only update the animating state for our own transition (not a child's)
    if (event.target === event.currentTarget) {
      this.setState({ isAnimating: false });
    }
  }

  renderIcon() {
    const { icon } = this.props;
    if (icon && icon.startsWith("moz-extension://")) {
      return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("span", { className: "icon icon-small-spacer", style: { backgroundImage: `url('${icon}')` } });
    }
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("span", { className: `icon icon-small-spacer icon-${icon || "webextension"}` });
  }

  onMenuButtonClick(event) {
    event.preventDefault();
    this.setState({ showContextMenu: true });
  }

  onMenuButtonMouseEnter() {
    this.setState({ menuButtonHover: true });
  }

  onMenuButtonMouseLeave() {
    this.setState({ menuButtonHover: false });
  }

  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }

  render() {
    const isCollapsible = this.props.collapsed !== undefined;
    const { enableAnimation, isAnimating, maxHeight, menuButtonHover, showContextMenu } = this.state;
    const { id, eventSource, collapsed, disclaimer, title, extraMenuOptions, showPrefName, privacyNoticeURL, dispatch, isFirst, isLast, isWebExtension } = this.props;
    const disclaimerPref = `section.${id}.showDisclaimer`;
    const needsDisclaimer = disclaimer && this.props.Prefs.values[disclaimerPref];
    const active = menuButtonHover || showContextMenu;
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "section",
      {
        className: `collapsible-section ${this.props.className}${enableAnimation ? " animation-enabled" : ""}${collapsed ? " collapsed" : ""}${active ? " active" : ""}`
        // Note: data-section-id is used for web extension api tests in mozilla central
        , "data-section-id": id },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "div",
        { className: "section-top-bar" },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "h3",
          { className: "section-title" },
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "span",
            { className: "click-target", onClick: this.onHeaderClick },
            this.renderIcon(),
            getFormattedMessage(title),
            isCollapsible && __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("span", { className: `collapsible-arrow icon ${collapsed ? "icon-arrowhead-forward-small" : "icon-arrowhead-down-small"}` })
          )
        ),
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "div",
          null,
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "button",
            {
              className: "context-menu-button icon",
              onClick: this.onMenuButtonClick,
              onMouseEnter: this.onMenuButtonMouseEnter,
              onMouseLeave: this.onMenuButtonMouseLeave },
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "span",
              { className: "sr-only" },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "section_context_menu_button_sr" })
            )
          ),
          showContextMenu && __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_4_content_src_components_SectionMenu_SectionMenu__["a" /* SectionMenu */], {
            id: id,
            extraOptions: extraMenuOptions,
            eventSource: eventSource,
            showPrefName: showPrefName,
            privacyNoticeURL: privacyNoticeURL,
            collapsed: collapsed,
            onUpdate: this.onMenuUpdate,
            isFirst: isFirst,
            isLast: isLast,
            dispatch: dispatch,
            isWebExtension: isWebExtension })
        )
      ),
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        __WEBPACK_IMPORTED_MODULE_2_content_src_components_ErrorBoundary_ErrorBoundary__["a" /* ErrorBoundary */],
        { className: "section-body-fallback" },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "div",
          {
            className: `section-body${isAnimating ? " animating" : ""}`,
            onTransitionEnd: this.onTransitionEnd,
            ref: this.onBodyMount,
            style: isAnimating && !collapsed ? { maxHeight } : null },
          needsDisclaimer && __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(DisclaimerIntl, { disclaimerPref: disclaimerPref, disclaimer: disclaimer, eventSource: eventSource, dispatch: this.props.dispatch }),
          this.props.children
        )
      )
    );
  }
}
/* unused harmony export _CollapsibleSection */


_CollapsibleSection.defaultProps = {
  document: global.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden"
  },
  Prefs: { values: {} }
};

const CollapsibleSection = Object(__WEBPACK_IMPORTED_MODULE_0_react_intl__["injectIntl"])(_CollapsibleSection);
/* harmony export (immutable) */ __webpack_exports__["a"] = CollapsibleSection;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 15 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);


/**
 * List of functions that return items that can be included as menu options in a
 * SectionMenu. All functions take the section as the only parameter.
 */
const SectionMenuOptions = {
  Separator: () => ({ type: "separator" }),
  MoveUp: section => ({
    id: "section_menu_action_move_up",
    icon: "arrowhead-up",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SECTION_MOVE,
      data: { id: section.id, direction: -1 }
    }),
    userEvent: "MENU_MOVE_UP",
    disabled: !!section.isFirst
  }),
  MoveDown: section => ({
    id: "section_menu_action_move_down",
    icon: "arrowhead-down",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SECTION_MOVE,
      data: { id: section.id, direction: +1 }
    }),
    userEvent: "MENU_MOVE_DOWN",
    disabled: !!section.isLast
  }),
  RemoveSection: section => ({
    id: "section_menu_action_remove_section",
    icon: "dismiss",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].SetPref(section.showPrefName, false),
    userEvent: "MENU_REMOVE"
  }),
  CollapseSection: section => ({
    id: "section_menu_action_collapse_section",
    icon: "minimize",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].UPDATE_SECTION_PREFS, data: { id: section.id, value: { collapsed: true } } }),
    userEvent: "MENU_COLLAPSE"
  }),
  ExpandSection: section => ({
    id: "section_menu_action_expand_section",
    icon: "maximize",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].UPDATE_SECTION_PREFS, data: { id: section.id, value: { collapsed: false } } }),
    userEvent: "MENU_EXPAND"
  }),
  ManageSection: section => ({
    id: "section_menu_action_manage_section",
    icon: "settings",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SETTINGS_OPEN }),
    userEvent: "MENU_MANAGE"
  }),
  ManageWebExtension: section => ({
    id: "section_menu_action_manage_webext",
    icon: "settings",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_WEBEXT_SETTINGS, data: section.id })
  }),
  AddTopSite: section => ({
    id: "section_menu_action_add_topsite",
    icon: "add",
    action: { type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_EDIT, data: { index: -1 } },
    userEvent: "MENU_ADD_TOPSITE"
  }),
  PrivacyNotice: section => ({
    id: "section_menu_action_privacy_notice",
    icon: "info",
    action: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_LINK,
      data: { url: section.privacyNoticeURL }
    }),
    userEvent: "MENU_PRIVACY_NOTICE"
  }),
  CheckCollapsed: section => section.collapsed ? SectionMenuOptions.ExpandSection(section) : SectionMenuOptions.CollapseSection(section)
};
/* harmony export (immutable) */ __webpack_exports__["a"] = SectionMenuOptions;


/***/ }),
/* 16 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_common_PerfService_jsm__ = __webpack_require__(17);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react__);




// Currently record only a fixed set of sections. This will prevent data
// from custom sections from showing up or from topstories.
const RECORDED_SECTIONS = ["highlights", "topsites"];

class ComponentPerfTimer extends __WEBPACK_IMPORTED_MODULE_2_react___default.a.Component {
  constructor(props) {
    super(props);
    // Just for test dependency injection:
    this.perfSvc = this.props.perfSvc || __WEBPACK_IMPORTED_MODULE_1_common_PerfService_jsm__["a" /* perfService */];

    this._sendBadStateEvent = this._sendBadStateEvent.bind(this);
    this._sendPaintedEvent = this._sendPaintedEvent.bind(this);
    this._reportMissingData = false;
    this._timestampHandled = false;
    this._recordedFirstRender = false;
  }

  componentDidMount() {
    if (!RECORDED_SECTIONS.includes(this.props.id)) {
      return;
    }

    this._maybeSendPaintedEvent();
  }

  componentDidUpdate() {
    if (!RECORDED_SECTIONS.includes(this.props.id)) {
      return;
    }

    this._maybeSendPaintedEvent();
  }

  /**
   * Call the given callback after the upcoming frame paints.
   *
   * @note Both setTimeout and requestAnimationFrame are throttled when the page
   * is hidden, so this callback may get called up to a second or so after the
   * requestAnimationFrame "paint" for hidden tabs.
   *
   * Newtabs hidden while loading will presumably be fairly rare (other than
   * preloaded tabs, which we will be filtering out on the server side), so such
   * cases should get lost in the noise.
   *
   * If we decide that it's important to find out when something that's hidden
   * has "painted", however, another option is to post a message to this window.
   * That should happen even faster than setTimeout, and, at least as of this
   * writing, it's not throttled in hidden windows in Firefox.
   *
   * @param {Function} callback
   *
   * @returns void
   */
  _afterFramePaint(callback) {
    requestAnimationFrame(() => setTimeout(callback, 0));
  }

  _maybeSendBadStateEvent() {
    // Follow up bugs:
    // https://github.com/mozilla/activity-stream/issues/3691
    if (!this.props.initialized) {
      // Remember to report back when data is available.
      this._reportMissingData = true;
    } else if (this._reportMissingData) {
      this._reportMissingData = false;
      // Report how long it took for component to become initialized.
      this._sendBadStateEvent();
    }
  }

  _maybeSendPaintedEvent() {
    // If we've already handled a timestamp, don't do it again.
    if (this._timestampHandled || !this.props.initialized) {
      return;
    }

    // And if we haven't, we're doing so now, so remember that. Even if
    // something goes wrong in the callback, we can't try again, as we'd be
    // sending back the wrong data, and we have to do it here, so that other
    // calls to this method while waiting for the next frame won't also try to
    // handle it.
    this._timestampHandled = true;
    this._afterFramePaint(this._sendPaintedEvent);
  }

  /**
   * Triggered by call to render. Only first call goes through due to
   * `_recordedFirstRender`.
   */
  _ensureFirstRenderTsRecorded() {
    // Used as t0 for recording how long component took to initialize.
    if (!this._recordedFirstRender) {
      this._recordedFirstRender = true;
      // topsites_first_render_ts, highlights_first_render_ts.
      const key = `${this.props.id}_first_render_ts`;
      this.perfSvc.mark(key);
    }
  }

  /**
   * Creates `TELEMETRY_UNDESIRED_EVENT` with timestamp in ms
   * of how much longer the data took to be ready for display than it would
   * have been the ideal case.
   * https://github.com/mozilla/ping-centre/issues/98
   */
  _sendBadStateEvent() {
    // highlights_data_ready_ts, topsites_data_ready_ts.
    const dataReadyKey = `${this.props.id}_data_ready_ts`;
    this.perfSvc.mark(dataReadyKey);

    try {
      const firstRenderKey = `${this.props.id}_first_render_ts`;
      // value has to be Int32.
      const value = parseInt(this.perfSvc.getMostRecentAbsMarkStartByName(dataReadyKey) - this.perfSvc.getMostRecentAbsMarkStartByName(firstRenderKey), 10);
      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
        type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_SESSION_PERF_DATA,
        // highlights_data_late_by_ms, topsites_data_late_by_ms.
        data: { [`${this.props.id}_data_late_by_ms`]: value }
      }));
    } catch (ex) {
      // If this failed, it's likely because the `privacy.resistFingerprinting`
      // pref is true.
    }
  }

  _sendPaintedEvent() {
    // Record first_painted event but only send if topsites.
    if (this.props.id !== "topsites") {
      return;
    }

    // topsites_first_painted_ts.
    const key = `${this.props.id}_first_painted_ts`;
    this.perfSvc.mark(key);

    try {
      const data = {};
      data[key] = this.perfSvc.getMostRecentAbsMarkStartByName(key);

      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
        type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_SESSION_PERF_DATA,
        data
      }));
    } catch (ex) {
      // If this failed, it's likely because the `privacy.resistFingerprinting`
      // pref is true.  We should at least not blow up, and should continue
      // to set this._timestampHandled to avoid going through this again.
    }
  }

  render() {
    if (RECORDED_SECTIONS.includes(this.props.id)) {
      this._ensureFirstRenderTsRecorded();
      this._maybeSendBadStateEvent();
    }
    return this.props.children;
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = ComponentPerfTimer;


/***/ }),
/* 17 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* unused harmony export _PerfService */
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "a", function() { return perfService; });
/* globals Services */


/* istanbul ignore if */

if (typeof ChromeUtils !== "undefined") {
  ChromeUtils.import("resource://gre/modules/Services.jsm");
}

let usablePerfObj;

/* istanbul ignore if */
/* istanbul ignore else */
if (typeof Services !== "undefined") {
  // Borrow the high-resolution timer from the hidden window....
  usablePerfObj = Services.appShell.hiddenDOMWindow.performance;
} else if (typeof performance !== "undefined") {
  // we must be running in content space
  // eslint-disable-next-line no-undef
  usablePerfObj = performance;
} else {
  // This is a dummy object so this file doesn't crash in the node prerendering
  // task.
  usablePerfObj = {
    now() {},
    mark() {}
  };
}

function _PerfService(options) {
  // For testing, so that we can use a fake Window.performance object with
  // known state.
  if (options && options.performanceObj) {
    this._perf = options.performanceObj;
  } else {
    this._perf = usablePerfObj;
  }
}


_PerfService.prototype = {
  /**
   * Calls the underlying mark() method on the appropriate Window.performance
   * object to add a mark with the given name to the appropriate performance
   * timeline.
   *
   * @param  {String} name  the name to give the current mark
   * @return {void}
   */
  mark: function mark(str) {
    this._perf.mark(str);
  },

  /**
   * Calls the underlying getEntriesByName on the appropriate Window.performance
   * object.
   *
   * @param  {String} name
   * @param  {String} type eg "mark"
   * @return {Array}       Performance* objects
   */
  getEntriesByName: function getEntriesByName(name, type) {
    return this._perf.getEntriesByName(name, type);
  },

  /**
   * The timeOrigin property from the appropriate performance object.
   * Used to ensure that timestamps from the add-on code and the content code
   * are comparable.
   *
   * @note If this is called from a context without a window
   * (eg a JSM in chrome), it will return the timeOrigin of the XUL hidden
   * window, which appears to be the first created window (and thus
   * timeOrigin) in the browser.  Note also, however, there is also a private
   * hidden window, presumably for private browsing, which appears to be
   * created dynamically later.  Exactly how/when that shows up needs to be
   * investigated.
   *
   * @return {Number} A double of milliseconds with a precision of 0.5us.
   */
  get timeOrigin() {
    return this._perf.timeOrigin;
  },

  /**
   * Returns the "absolute" version of performance.now(), i.e. one that
   * should ([bug 1401406](https://bugzilla.mozilla.org/show_bug.cgi?id=1401406)
   * be comparable across both chrome and content.
   *
   * @return {Number}
   */
  absNow: function absNow() {
    return this.timeOrigin + this._perf.now();
  },

  /**
   * This returns the absolute startTime from the most recent performance.mark()
   * with the given name.
   *
   * @param  {String} name  the name to lookup the start time for
   *
   * @return {Number}       the returned start time, as a DOMHighResTimeStamp
   *
   * @throws {Error}        "No Marks with the name ..." if none are available
   *
   * @note Always surround calls to this by try/catch.  Otherwise your code
   * may fail when the `privacy.resistFingerprinting` pref is true.  When
   * this pref is set, all attempts to get marks will likely fail, which will
   * cause this method to throw.
   *
   * See [bug 1369303](https://bugzilla.mozilla.org/show_bug.cgi?id=1369303)
   * for more info.
   */
  getMostRecentAbsMarkStartByName(name) {
    let entries = this.getEntriesByName(name, "mark");

    if (!entries.length) {
      throw new Error(`No marks with the name ${name}`);
    }

    let mostRecentEntry = entries[entries.length - 1];
    return this._perf.timeOrigin + mostRecentEntry.startTime;
  }
};

var perfService = new _PerfService();

/***/ }),
/* 18 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__ = __webpack_require__(5);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_components_LinkMenu_LinkMenu__ = __webpack_require__(12);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_4_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_common_Reducers_jsm__ = __webpack_require__(6);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };








class TopSiteLink extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onDragEvent = this.onDragEvent.bind(this);
  }

  /*
   * Helper to determine whether the drop zone should allow a drop. We only allow
   * dropping top sites for now.
   */
  _allowDrop(e) {
    return e.dataTransfer.types.includes("text/topsite-index");
  }

  onDragEvent(event) {
    switch (event.type) {
      case "click":
        // Stop any link clicks if we started any dragging
        if (this.dragged) {
          event.preventDefault();
        }
        break;
      case "dragstart":
        this.dragged = true;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/topsite-index", this.props.index);
        event.target.blur();
        this.props.onDragEvent(event, this.props.index, this.props.link, this.props.title);
        break;
      case "dragend":
        this.props.onDragEvent(event);
        break;
      case "dragenter":
      case "dragover":
      case "drop":
        if (this._allowDrop(event)) {
          event.preventDefault();
          this.props.onDragEvent(event, this.props.index);
        }
        break;
      case "mousedown":
        // Reset at the first mouse event of a potential drag
        this.dragged = false;
        break;
    }
  }

  render() {
    const { children, className, defaultStyle, isDraggable, link, onClick, title } = this.props;
    const topSiteOuterClassName = `top-site-outer${className ? ` ${className}` : ""}${link.isDragged ? " dragged" : ""}`;
    const { tippyTopIcon, faviconSize } = link;
    const [letterFallback] = title;
    let imageClassName;
    let imageStyle;
    let showSmallFavicon = false;
    let smallFaviconStyle;
    let smallFaviconFallback;
    if (defaultStyle) {
      // force no styles (letter fallback) even if the link has imagery
      smallFaviconFallback = false;
    } else if (link.customScreenshotURL) {
      // assume high quality custom screenshot and use rich icon styles and class names
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${link.screenshot})`
      };
    } else if (tippyTopIcon || faviconSize >= __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["b" /* MIN_RICH_FAVICON_SIZE */]) {
      // styles and class names for top sites with rich icons
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon || link.favicon})`
      };
    } else {
      // styles and class names for top sites with screenshot + small icon in top left corner
      imageClassName = `screenshot${link.screenshot ? " active" : ""}`;
      imageStyle = { backgroundImage: link.screenshot ? `url(${link.screenshot})` : "none" };

      // only show a favicon in top left if it's greater than 16x16
      if (faviconSize >= __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["a" /* MIN_CORNER_FAVICON_SIZE */]) {
        showSmallFavicon = true;
        smallFaviconStyle = { backgroundImage: `url(${link.favicon})` };
      } else if (link.screenshot) {
        // Don't show a small favicon if there is no screenshot, because that
        // would result in two fallback icons
        showSmallFavicon = true;
        smallFaviconFallback = true;
      }
    }
    let draggableProps = {};
    if (isDraggable) {
      draggableProps = {
        onClick: this.onDragEvent,
        onDragEnd: this.onDragEvent,
        onDragStart: this.onDragEvent,
        onMouseDown: this.onDragEvent
      };
    }
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      "li",
      _extends({ className: topSiteOuterClassName, onDrop: this.onDragEvent, onDragOver: this.onDragEvent, onDragEnter: this.onDragEvent, onDragLeave: this.onDragEvent }, draggableProps),
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
        "div",
        { className: "top-site-inner" },
        __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
          "a",
          { href: link.url, onClick: onClick },
          __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
            "div",
            { className: "tile", "aria-hidden": true, "data-fallback": letterFallback },
            __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement("div", { className: imageClassName, style: imageStyle }),
            showSmallFavicon && __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement("div", {
              className: "top-site-icon default-icon",
              "data-fallback": smallFaviconFallback && letterFallback,
              style: smallFaviconStyle })
          ),
          __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
            "div",
            { className: `title ${link.isPinned ? "pinned" : ""}` },
            link.isPinned && __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement("div", { className: "icon icon-pin-small" }),
            __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
              "span",
              { dir: "auto" },
              title
            )
          )
        ),
        children
      )
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = TopSiteLink;

TopSiteLink.defaultProps = {
  title: "",
  link: {},
  isDraggable: true
};

class TopSite extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false };
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
  }

  /**
   * Report to telemetry additional information about the item.
   */
  _getTelemetryInfo() {
    const value = { icon_type: this.props.link.iconType };
    // Filter out "not_pinned" type for being the default
    if (this.props.link.isPinned) {
      value.card_type = "pinned";
    }
    return { value };
  }

  userEvent(event) {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent(Object.assign({
      event,
      source: __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["d" /* TOP_SITES_SOURCE */],
      action_position: this.props.index
    }, this._getTelemetryInfo())));
  }

  onLinkClick(event) {
    this.userEvent("CLICK");

    // Specially handle a top site link click for "typed" frecency bonus as
    // specified as a property on the link.
    event.preventDefault();
    const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].OPEN_LINK,
      data: Object.assign(this.props.link, { event: { altKey, button, ctrlKey, metaKey, shiftKey } })
    }));
  }

  onMenuButtonClick(event) {
    event.preventDefault();
    this.props.onActivate(this.props.index);
    this.setState({ showContextMenu: true });
  }

  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }

  render() {
    const { props } = this;
    const { link } = props;
    const isContextMenuOpen = this.state.showContextMenu && props.activeIndex === props.index;
    const title = link.label || link.hostname;
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      TopSiteLink,
      _extends({}, props, { onClick: this.onLinkClick, onDragEvent: this.props.onDragEvent, className: `${props.className || ""}${isContextMenuOpen ? " active" : ""}`, title: title }),
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
        "div",
        null,
        __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
          "button",
          { className: "context-menu-button icon", onClick: this.onMenuButtonClick },
          __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
            "span",
            { className: "sr-only" },
            __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_1_react_intl__["FormattedMessage"], { id: "context_menu_button_sr", values: { title } })
          )
        ),
        isContextMenuOpen && __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_3_content_src_components_LinkMenu_LinkMenu__["a" /* LinkMenu */], {
          dispatch: props.dispatch,
          index: props.index,
          onUpdate: this.onMenuUpdate,
          options: __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["c" /* TOP_SITES_CONTEXT_MENU_OPTIONS */],
          site: link,
          siteInfo: this._getTelemetryInfo(),
          source: __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["d" /* TOP_SITES_SOURCE */] })
      )
    );
  }
}
/* unused harmony export TopSite */

TopSite.defaultProps = {
  link: {},
  onActivate() {}
};

class TopSitePlaceholder extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
  }

  onEditButtonClick() {
    this.props.dispatch({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_EDIT, data: { index: this.props.index } });
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      TopSiteLink,
      _extends({}, this.props, { className: `placeholder ${this.props.className || ""}`, isDraggable: false }),
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement("button", { className: "context-menu-button edit-button icon",
        title: this.props.intl.formatMessage({ id: "edit_topsites_edit_button" }),
        onClick: this.onEditButtonClick })
    );
  }
}
/* unused harmony export TopSitePlaceholder */


class _TopSiteList extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  static get DEFAULT_STATE() {
    return {
      activeIndex: null,
      draggedIndex: null,
      draggedSite: null,
      draggedTitle: null,
      topSitesPreview: null
    };
  }

  constructor(props) {
    super(props);
    this.state = _TopSiteList.DEFAULT_STATE;
    this.onDragEvent = this.onDragEvent.bind(this);
    this.onActivate = this.onActivate.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (this.state.draggedSite) {
      const prevTopSites = this.props.TopSites && this.props.TopSites.rows;
      const newTopSites = nextProps.TopSites && nextProps.TopSites.rows;
      if (prevTopSites && prevTopSites[this.state.draggedIndex] && prevTopSites[this.state.draggedIndex].url === this.state.draggedSite.url && (!newTopSites[this.state.draggedIndex] || newTopSites[this.state.draggedIndex].url !== this.state.draggedSite.url)) {
        // We got the new order from the redux store via props. We can clear state now.
        this.setState(_TopSiteList.DEFAULT_STATE);
      }
    }
  }

  userEvent(event, index) {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({
      event,
      source: __WEBPACK_IMPORTED_MODULE_2__TopSitesConstants__["d" /* TOP_SITES_SOURCE */],
      action_position: index
    }));
  }

  onDragEvent(event, index, link, title) {
    switch (event.type) {
      case "dragstart":
        this.dropped = false;
        this.setState({
          draggedIndex: index,
          draggedSite: link,
          draggedTitle: title,
          activeIndex: null
        });
        this.userEvent("DRAG", index);
        break;
      case "dragend":
        if (!this.dropped) {
          // If there was no drop event, reset the state to the default.
          this.setState(_TopSiteList.DEFAULT_STATE);
        }
        break;
      case "dragenter":
        if (index === this.state.draggedIndex) {
          this.setState({ topSitesPreview: null });
        } else {
          this.setState({ topSitesPreview: this._makeTopSitesPreview(index) });
        }
        break;
      case "drop":
        if (index !== this.state.draggedIndex) {
          this.dropped = true;
          this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
            type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_INSERT,
            data: {
              site: {
                url: this.state.draggedSite.url,
                label: this.state.draggedTitle,
                customScreenshotURL: this.state.draggedSite.customScreenshotURL
              },
              index,
              draggedFromIndex: this.state.draggedIndex
            }
          }));
          this.userEvent("DROP", index);
        }
        break;
    }
  }

  _getTopSites() {
    // Make a copy of the sites to truncate or extend to desired length
    let topSites = this.props.TopSites.rows.slice();
    topSites.length = this.props.TopSitesRows * __WEBPACK_IMPORTED_MODULE_5_common_Reducers_jsm__["a" /* TOP_SITES_MAX_SITES_PER_ROW */];
    return topSites;
  }

  /**
   * Make a preview of the topsites that will be the result of dropping the currently
   * dragged site at the specified index.
   */
  _makeTopSitesPreview(index) {
    const topSites = this._getTopSites();
    topSites[this.state.draggedIndex] = null;
    const pinnedOnly = topSites.map(site => site && site.isPinned ? site : null);
    const unpinned = topSites.filter(site => site && !site.isPinned);
    const siteToInsert = Object.assign({}, this.state.draggedSite, { isPinned: true, isDragged: true });
    if (!pinnedOnly[index]) {
      pinnedOnly[index] = siteToInsert;
    } else {
      // Find the hole to shift the pinned site(s) towards. We shift towards the
      // hole left by the site being dragged.
      let holeIndex = index;
      const indexStep = index > this.state.draggedIndex ? -1 : 1;
      while (pinnedOnly[holeIndex]) {
        holeIndex += indexStep;
      }

      // Shift towards the hole.
      const shiftingStep = index > this.state.draggedIndex ? 1 : -1;
      while (holeIndex !== index) {
        const nextIndex = holeIndex + shiftingStep;
        pinnedOnly[holeIndex] = pinnedOnly[nextIndex];
        holeIndex = nextIndex;
      }
      pinnedOnly[index] = siteToInsert;
    }

    // Fill in the remaining holes with unpinned sites.
    const preview = pinnedOnly;
    for (let i = 0; i < preview.length; i++) {
      if (!preview[i]) {
        preview[i] = unpinned.shift() || null;
      }
    }

    return preview;
  }

  onActivate(index) {
    this.setState({ activeIndex: index });
  }

  render() {
    const { props } = this;
    const topSites = this.state.topSitesPreview || this._getTopSites();
    const topSitesUI = [];
    const commonProps = {
      onDragEvent: this.onDragEvent,
      dispatch: props.dispatch,
      intl: props.intl
    };
    // We assign a key to each placeholder slot. We need it to be independent
    // of the slot index (i below) so that the keys used stay the same during
    // drag and drop reordering and the underlying DOM nodes are reused.
    // This mostly (only?) affects linux so be sure to test on linux before changing.
    let holeIndex = 0;

    // On narrow viewports, we only show 6 sites per row. We'll mark the rest as
    // .hide-for-narrow to hide in CSS via @media query.
    const maxNarrowVisibleIndex = props.TopSitesRows * 6;

    for (let i = 0, l = topSites.length; i < l; i++) {
      const link = topSites[i] && Object.assign({}, topSites[i], { iconType: this.props.topSiteIconType(topSites[i]) });
      const slotProps = {
        key: link ? link.url : holeIndex++,
        index: i
      };
      if (i >= maxNarrowVisibleIndex) {
        slotProps.className = "hide-for-narrow";
      }
      topSitesUI.push(!link ? __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(TopSitePlaceholder, _extends({}, slotProps, commonProps)) : __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(TopSite, _extends({
        link: link,
        activeIndex: this.state.activeIndex,
        onActivate: this.onActivate
      }, slotProps, commonProps)));
    }
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      "ul",
      { className: `top-sites-list${this.state.draggedSite ? " dnd-active" : ""}` },
      topSitesUI
    );
  }
}
/* unused harmony export _TopSiteList */


const TopSiteList = Object(__WEBPACK_IMPORTED_MODULE_1_react_intl__["injectIntl"])(_TopSiteList);
/* harmony export (immutable) */ __webpack_exports__["b"] = TopSiteList;


/***/ }),
/* 19 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
Object.defineProperty(__webpack_exports__, "__esModule", { value: true });
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_content_src_lib_snippets__ = __webpack_require__(20);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_content_src_components_Base_Base__ = __webpack_require__(25);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_lib_detect_user_session_start__ = __webpack_require__(39);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_content_src_lib_init_store__ = __webpack_require__(8);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_5_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_6_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_7_react_dom__ = __webpack_require__(9);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_7_react_dom___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_7_react_dom__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_8_common_Reducers_jsm__ = __webpack_require__(6);










const store = Object(__WEBPACK_IMPORTED_MODULE_4_content_src_lib_init_store__["b" /* initStore */])(__WEBPACK_IMPORTED_MODULE_8_common_Reducers_jsm__["b" /* reducers */], global.gActivityStreamPrerenderedState);

new __WEBPACK_IMPORTED_MODULE_3_content_src_lib_detect_user_session_start__["a" /* DetectUserSessionStart */](store).sendEventOrAddListener();

// If we are starting in a prerendered state, we must wait until the first render
// to request state rehydration (see Base.jsx). If we are NOT in a prerendered state,
// we can request it immedately.
if (!global.gActivityStreamPrerenderedState) {
  store.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].NEW_TAB_STATE_REQUEST }));
}

__WEBPACK_IMPORTED_MODULE_7_react_dom___default.a.hydrate(__WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
  __WEBPACK_IMPORTED_MODULE_5_react_redux__["Provider"],
  { store: store },
  __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_content_src_components_Base_Base__["a" /* Base */], {
    isFirstrun: global.document.location.href === "about:welcome",
    isPrerendered: !!global.gActivityStreamPrerenderedState,
    locale: global.document.documentElement.lang,
    strings: global.gActivityStreamStrings })
), document.getElementById("root"));

Object(__WEBPACK_IMPORTED_MODULE_1_content_src_lib_snippets__["a" /* addSnippetsSubscriber */])(store);
/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 20 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (immutable) */ __webpack_exports__["a"] = addSnippetsSubscriber;
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_content_src_asrouter_asrouter_content__ = __webpack_require__(7);
const DATABASE_NAME = "snippets_db";
const DATABASE_VERSION = 1;
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
const SNIPPETS_UPDATE_INTERVAL_MS = 14400000;
/* unused harmony export SNIPPETS_UPDATE_INTERVAL_MS */
 // 4 hours.

const SNIPPETS_ENABLED_EVENT = "Snippets:Enabled";
const SNIPPETS_DISABLED_EVENT = "Snippets:Disabled";




/**
 * SnippetsMap - A utility for cacheing values related to the snippet. It has
 *               the same interface as a Map, but is optionally backed by
 *               indexedDB for persistent storage.
 *               Call .connect() to open a database connection and restore any
 *               previously cached data, if necessary.
 *
 */
class SnippetsMap extends Map {
  constructor(dispatch) {
    super();
    this._db = null;
    this._dispatch = dispatch;
  }

  set(key, value) {
    super.set(key, value);
    return this._dbTransaction(db => db.put(value, key));
  }

  delete(key) {
    super.delete(key);
    return this._dbTransaction(db => db.delete(key));
  }

  clear() {
    super.clear();
    this._dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SNIPPETS_BLOCKLIST_CLEARED }));
    return this._dbTransaction(db => db.clear());
  }

  get blockList() {
    return this.get("blockList") || [];
  }

  /**
   * blockSnippetById - Blocks a snippet given an id
   *
   * @param  {str|int} id   The id of the snippet
   * @return {Promise}      Resolves when the id has been written to indexedDB,
   *                        or immediately if the snippetMap is not connected
   */
  async blockSnippetById(id) {
    if (!id) {
      return;
    }
    const { blockList } = this;
    if (!blockList.includes(id)) {
      blockList.push(id);
      this._dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SNIPPETS_BLOCKLIST_UPDATED, data: id }));
      await this.set("blockList", blockList);
    }
  }

  disableOnboarding() {
    this._dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DISABLE_ONBOARDING }));
  }

  showFirefoxAccounts() {
    this._dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SHOW_FIREFOX_ACCOUNTS }));
  }

  getTotalBookmarksCount() {
    return new Promise(resolve => {
      this._dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOTAL_BOOKMARKS_REQUEST }));
      global.addMessageListener("ActivityStream:MainToContent", function onMessage({ data: action }) {
        if (action.type === __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOTAL_BOOKMARKS_RESPONSE) {
          resolve(action.data);
          global.removeMessageListener("ActivityStream:MainToContent", onMessage);
        }
      });
    });
  }

  /**
   * connect - Attaches an indexedDB back-end to the Map so that any set values
   *           are also cached in a store. It also restores any existing values
   *           that are already stored in the indexedDB store.
   *
   * @return {type}  description
   */
  async connect() {
    // Open the connection
    const db = await this._openDB();

    // Restore any existing values
    await this._restoreFromDb(db);

    // Attach a reference to the db
    this._db = db;
  }

  /**
   * _dbTransaction - Returns a db transaction wrapped with the given modifier
   *                  function as a Promise. If the db has not been connected,
   *                  it resolves immediately.
   *
   * @param  {func} modifier A function to call with the transaction
   * @return {obj}           A Promise that resolves when the transaction has
   *                         completed or errored
   */
  _dbTransaction(modifier) {
    if (!this._db) {
      return Promise.resolve();
    }
    return new Promise((resolve, reject) => {
      const transaction = modifier(this._db.transaction(SNIPPETS_OBJECTSTORE_NAME, "readwrite").objectStore(SNIPPETS_OBJECTSTORE_NAME));
      transaction.onsuccess = event => resolve();

      /* istanbul ignore next */
      transaction.onerror = event => reject(transaction.error);
    });
  }

  _openDB() {
    return new Promise((resolve, reject) => {
      const openRequest = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);

      /* istanbul ignore next */
      openRequest.onerror = event => {
        // Try to delete the old database so that we can start this process over
        // next time.
        indexedDB.deleteDatabase(DATABASE_NAME);
        reject(event);
      };

      openRequest.onupgradeneeded = event => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains(SNIPPETS_OBJECTSTORE_NAME)) {
          db.createObjectStore(SNIPPETS_OBJECTSTORE_NAME);
        }
      };

      openRequest.onsuccess = event => {
        let db = event.target.result;

        /* istanbul ignore next */
        db.onerror = err => console.error(err); // eslint-disable-line no-console
        /* istanbul ignore next */
        db.onversionchange = versionChangeEvent => versionChangeEvent.target.close();

        resolve(db);
      };
    });
  }

  _restoreFromDb(db) {
    return new Promise((resolve, reject) => {
      let cursorRequest;
      try {
        cursorRequest = db.transaction(SNIPPETS_OBJECTSTORE_NAME).objectStore(SNIPPETS_OBJECTSTORE_NAME).openCursor();
      } catch (err) {
        // istanbul ignore next
        reject(err);
        // istanbul ignore next
        return;
      }

      /* istanbul ignore next */
      cursorRequest.onerror = event => reject(event);

      cursorRequest.onsuccess = event => {
        let cursor = event.target.result;
        // Populate the cache from the persistent storage.
        if (cursor) {
          if (cursor.value !== "blockList") {
            this.set(cursor.key, cursor.value);
          }
          cursor.continue();
        } else {
          // We are done.
          resolve();
        }
      };
    });
  }
}
/* unused harmony export SnippetsMap */


/**
 * SnippetsProvider - Initializes a SnippetsMap and loads snippets from a
 *                    remote location, or else default snippets if the remote
 *                    snippets cannot be retrieved.
 */
class SnippetsProvider {
  constructor(dispatch) {
    // Initialize the Snippets Map and attaches it to a global so that
    // the snippet payload can interact with it.
    global.gSnippetsMap = new SnippetsMap(dispatch);
    this._onAction = this._onAction.bind(this);
  }

  get snippetsMap() {
    return global.gSnippetsMap;
  }

  async _refreshSnippets() {
    // Check if the cached version of of the snippets in snippetsMap. If it's too
    // old, blow away the entire snippetsMap.
    const cachedVersion = this.snippetsMap.get("snippets-cached-version");

    if (cachedVersion !== this.appData.version) {
      this.snippetsMap.clear();
    }

    // Has enough time passed for us to require an update?
    const lastUpdate = this.snippetsMap.get("snippets-last-update");
    const needsUpdate = !(lastUpdate >= 0) || Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS;

    if (needsUpdate && this.appData.snippetsURL) {
      this.snippetsMap.set("snippets-last-update", Date.now());
      try {
        const response = await fetch(this.appData.snippetsURL);
        if (response.status === 200) {
          const payload = await response.text();

          this.snippetsMap.set("snippets", payload);
          this.snippetsMap.set("snippets-cached-version", this.appData.version);
        }
      } catch (e) {
        console.error(e); // eslint-disable-line no-console
      }
    }
  }

  _noSnippetFallback() {
    // TODO
  }

  _forceOnboardingVisibility(shouldBeVisible) {
    const onboardingEl = document.getElementById("onboarding-notification-bar");

    if (onboardingEl) {
      onboardingEl.style.display = shouldBeVisible ? "" : "none";
    }
  }

  _showRemoteSnippets() {
    const snippetsEl = document.getElementById(this.elementId);
    const payload = this.snippetsMap.get("snippets");

    if (!snippetsEl) {
      throw new Error(`No element was found with id '${this.elementId}'.`);
    }

    // This could happen if fetching failed
    if (!payload) {
      throw new Error("No remote snippets were found in gSnippetsMap.");
    }

    if (typeof payload !== "string") {
      throw new Error("Snippet payload was incorrectly formatted");
    }

    // Note that injecting snippets can throw if they're invalid XML.
    // eslint-disable-next-line no-unsanitized/property
    snippetsEl.innerHTML = payload;

    // Scripts injected by innerHTML are inactive, so we have to relocate them
    // through DOM manipulation to activate their contents.
    for (const scriptEl of snippetsEl.getElementsByTagName("script")) {
      const relocatedScript = document.createElement("script");
      relocatedScript.text = scriptEl.text;
      scriptEl.parentNode.replaceChild(relocatedScript, scriptEl);
    }
  }

  _onAction(msg) {
    if (msg.data.type === __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SNIPPET_BLOCKED) {
      if (!this.snippetsMap.blockList.includes(msg.data.data)) {
        this.snippetsMap.set("blockList", this.snippetsMap.blockList.concat(msg.data.data));
        document.getElementById("snippets-container").style.display = "none";
      }
    }
  }

  /**
   * init - Fetch the snippet payload and show snippets
   *
   * @param  {obj} options
   * @param  {str} options.appData.snippetsURL  The URL from which we fetch snippets
   * @param  {int} options.appData.version  The current snippets version
   * @param  {str} options.elementId  The id of the element in which to inject snippets
   * @param  {bool} options.connect  Should gSnippetsMap connect to indexedDB?
   */
  async init(options) {
    Object.assign(this, {
      appData: {},
      elementId: "snippets",
      connect: true
    }, options);

    // Add listener so we know when snippets are blocked on other pages
    if (global.addMessageListener) {
      global.addMessageListener("ActivityStream:MainToContent", this._onAction);
    }

    // TODO: Requires enabling indexedDB on newtab
    // Restore the snippets map from indexedDB
    if (this.connect) {
      try {
        await this.snippetsMap.connect();
      } catch (e) {
        console.error(e); // eslint-disable-line no-console
      }
    }

    // Cache app data values so they can be accessible from gSnippetsMap
    for (const key of Object.keys(this.appData)) {
      if (key === "blockList") {
        this.snippetsMap.set("blockList", this.appData[key]);
      } else {
        this.snippetsMap.set(`appData.${key}`, this.appData[key]);
      }
    }

    // Refresh snippets, if enough time has passed.
    await this._refreshSnippets();

    // Try showing remote snippets, falling back to defaults if necessary.
    try {
      this._showRemoteSnippets();
    } catch (e) {
      this._noSnippetFallback(e);
    }

    window.dispatchEvent(new Event(SNIPPETS_ENABLED_EVENT));

    this._forceOnboardingVisibility(true);
    this.initialized = true;
  }

  uninit() {
    window.dispatchEvent(new Event(SNIPPETS_DISABLED_EVENT));
    this._forceOnboardingVisibility(false);
    if (global.removeMessageListener) {
      global.removeMessageListener("ActivityStream:MainToContent", this._onAction);
    }
    this.initialized = false;
  }
}
/* unused harmony export SnippetsProvider */


/**
 * addSnippetsSubscriber - Creates a SnippetsProvider that Initializes
 *                         when the store has received the appropriate
 *                         Snippet data.
 *
 * @param  {obj} store   The redux store
 * @return {obj}         Returns the snippets instance and unsubscribe function
 */
function addSnippetsSubscriber(store) {
  const snippets = new SnippetsProvider(store.dispatch);

  let initializing = false;

  store.subscribe(async () => {
    const state = store.getState();
    // state.Prefs.values["feeds.snippets"]:  Should snippets be shown?
    // state.Snippets.initialized             Is the snippets data initialized?
    // snippets.initialized:                  Is SnippetsProvider currently initialised?
    if (state.Prefs.values["feeds.snippets"] &&
    // If the message center experiment is enabled, don't show snippets
    !state.Prefs.values.asrouterExperimentEnabled && !state.Prefs.values.disableSnippets && state.Snippets.initialized && !snippets.initialized &&
    // Don't call init multiple times
    !initializing && location.href !== "about:welcome") {
      initializing = true;
      await snippets.init({ appData: state.Snippets });
      initializing = false;
    } else if ((state.Prefs.values["feeds.snippets"] === false || state.Prefs.values.disableSnippets === true) && snippets.initialized) {
      snippets.uninit();
    }

    if (state.Prefs.values.asrouterExperimentEnabled) {
      Object(__WEBPACK_IMPORTED_MODULE_1_content_src_asrouter_asrouter_content__["b" /* initASRouter */])();
    }
  });

  // These values are returned for testing purposes
  return snippets;
}
/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 21 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 22 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react__);


const VISIBLE = "visible";
/* unused harmony export VISIBLE */

const VISIBILITY_CHANGE_EVENT = "visibilitychange";
/* unused harmony export VISIBILITY_CHANGE_EVENT */


/**
 * Component wrapper used to send telemetry pings on every impression.
 */
class ImpressionsWrapper extends __WEBPACK_IMPORTED_MODULE_0_react___default.a.PureComponent {
  // This sends an event when a user sees a set of new content. If content
  // changes while the page is hidden (i.e. preloaded or on a hidden tab),
  // only send the event if the page becomes visible again.
  sendImpressionOrAddListener() {
    if (this.props.document.visibilityState === VISIBLE) {
      this.props.sendImpression();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      // When the page becomes visible, send the impression stats ping if the section isn't collapsed.
      this._onVisibilityChange = () => {
        if (this.props.document.visibilityState === VISIBLE) {
          this.props.sendImpression();
          this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };
      this.props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  componentWillUnmount() {
    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  componentDidMount() {
    if (this.props.sendOnMount) {
      this.sendImpressionOrAddListener();
    }
  }

  componentDidUpdate(prevProps) {
    if (this.props.shouldSendImpressionOnUpdate(this.props, prevProps)) {
      this.sendImpressionOrAddListener();
    }
  }

  render() {
    return this.props.children;
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = ImpressionsWrapper;


ImpressionsWrapper.defaultProps = {
  document: global.document,
  sendOnMount: true
};
/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 23 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: external "React"
var external__React_ = __webpack_require__(0);
var external__React__default = /*#__PURE__*/__webpack_require__.n(external__React_);

// CONCATENATED MODULE: ./system-addon/content-src/asrouter/components/ModalOverlay/ModalOverlay.jsx


class ModalOverlay_ModalOverlay extends external__React__default.a.PureComponent {
  componentWillMount() {
    this.setState({ active: true });
    document.body.classList.add("modal-open");
  }

  componentWillUnmount() {
    document.body.classList.remove("modal-open");
    this.setState({ active: false });
  }

  render() {
    const { active } = this.state;
    const { title, button_label } = this.props;
    return external__React__default.a.createElement(
      "div",
      null,
      external__React__default.a.createElement("div", { className: `modalOverlayOuter ${active ? "active" : ""}` }),
      external__React__default.a.createElement(
        "div",
        { className: `modalOverlayInner ${active ? "active" : ""}` },
        external__React__default.a.createElement(
          "h2",
          null,
          " ",
          title,
          " "
        ),
        this.props.children,
        external__React__default.a.createElement(
          "div",
          { className: "footer" },
          external__React__default.a.createElement(
            "button",
            { onClick: this.props.onDoneButton, className: "button primary modalButton" },
            " ",
            button_label,
            " "
          )
        )
      )
    );
  }
}
// CONCATENATED MODULE: ./system-addon/content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.jsx
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };




class OnboardingMessage_OnboardingCard extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    const { props } = this;
    props.sendUserActionTelemetry({ event: "TRY_NOW", message_id: props.id });
    props.onAction(props.content);
  }

  render() {
    const { content } = this.props;
    return external__React__default.a.createElement(
      "div",
      { className: "onboardingMessage" },
      external__React__default.a.createElement("div", { className: `onboardingMessageImage ${content.icon}` }),
      external__React__default.a.createElement(
        "div",
        { className: "onboardingContent" },
        external__React__default.a.createElement(
          "span",
          null,
          external__React__default.a.createElement(
            "h3",
            null,
            " ",
            content.title,
            " "
          ),
          external__React__default.a.createElement(
            "p",
            null,
            " ",
            content.text,
            " "
          )
        ),
        external__React__default.a.createElement(
          "span",
          null,
          external__React__default.a.createElement(
            "button",
            { className: "button onboardingButton", onClick: this.onClick },
            " ",
            content.button_label,
            " "
          )
        )
      )
    );
  }
}

class OnboardingMessage_OnboardingMessage extends external__React__default.a.PureComponent {
  render() {
    const { props } = this;
    return external__React__default.a.createElement(
      ModalOverlay_ModalOverlay,
      _extends({}, props, { button_label: "Start Browsing", title: "Welcome to Firefox" }),
      external__React__default.a.createElement(
        "div",
        { className: "onboardingMessageContainer" },
        props.bundle.map(message => external__React__default.a.createElement(OnboardingMessage_OnboardingCard, _extends({ key: message.id, sendUserActionTelemetry: props.sendUserActionTelemetry, onAction: props.onAction }, message)))
      )
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = OnboardingMessage_OnboardingMessage;


/***/ }),
/* 24 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: external "React"
var external__React_ = __webpack_require__(0);
var external__React__default = /*#__PURE__*/__webpack_require__.n(external__React_);

// CONCATENATED MODULE: ./system-addon/content-src/asrouter/template-utils.js
function safeURI(url) {
  if (!url) {
    return "";
  }
  const { protocol } = new URL(url);
  const isAllowed = ["http:", "https:", "data:", "resource:", "chrome:"].includes(protocol);
  if (!isAllowed) {
    console.warn(`The protocol ${protocol} is not allowed for template URLs.`); // eslint-disable-line no-console
  }
  return isAllowed ? url : "";
}
// CONCATENATED MODULE: ./system-addon/content-src/asrouter/components/Button/Button.jsx



const Button = props => external__React__default.a.createElement(
  "a",
  { href: safeURI(props.url),
    onClick: props.onClick,
    className: props.className || "ASRouterButton" },
  props.children
);
// CONCATENATED MODULE: ./system-addon/content-src/asrouter/components/SnippetBase/SnippetBase.jsx


class SnippetBase_SnippetBase extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
  }

  onBlockClicked() {
    this.props.sendUserActionTelemetry({ event: "BLOCK" });
    this.props.onBlock();
  }

  render() {
    const { props } = this;

    const containerClassName = `SnippetBaseContainer${props.className ? ` ${props.className}` : ""}`;

    return external__React__default.a.createElement(
      "div",
      { className: containerClassName },
      external__React__default.a.createElement(
        "div",
        { className: "innerWrapper" },
        props.children
      ),
      external__React__default.a.createElement("button", { className: "blockButton", onClick: this.onBlockClicked })
    );
  }
}
// CONCATENATED MODULE: ./system-addon/content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };






const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png";

class SimpleSnippet_SimpleSnippet extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  onButtonClick() {
    this.props.sendUserActionTelemetry({ event: "CLICK_BUTTON" });
  }

  renderTitle() {
    const { title } = this.props.content;
    return title ? external__React__default.a.createElement(
      "h3",
      { className: "title" },
      title
    ) : null;
  }

  renderButton(className) {
    const { props } = this;
    return external__React__default.a.createElement(
      Button,
      {
        className: className,
        onClick: this.onButtonClick,
        url: props.content.button_url },
      props.content.button_label
    );
  }

  render() {
    const { props } = this;
    const hasLink = props.content.button_url && props.content.button_type === "anchor";
    const hasButton = props.content.button_url && !props.content.button_type;
    return external__React__default.a.createElement(
      SnippetBase_SnippetBase,
      _extends({}, props, { className: "SimpleSnippet" }),
      external__React__default.a.createElement("img", { src: safeURI(props.content.icon) || DEFAULT_ICON_PATH, className: "icon" }),
      external__React__default.a.createElement(
        "div",
        null,
        this.renderTitle(),
        " ",
        external__React__default.a.createElement(
          "p",
          { className: "body" },
          props.content.text
        ),
        " ",
        hasLink ? this.renderButton("ASRouterAnchor") : null
      ),
      hasButton ? external__React__default.a.createElement(
        "div",
        null,
        this.renderButton()
      ) : null
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = SimpleSnippet_SimpleSnippet;


/***/ }),
/* 25 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_content_src_components_ASRouterAdmin_ASRouterAdmin__ = __webpack_require__(26);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_components_ConfirmDialog_ConfirmDialog__ = __webpack_require__(27);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_4_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_content_src_components_ErrorBoundary_ErrorBoundary__ = __webpack_require__(10);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_content_src_components_ManualMigration_ManualMigration__ = __webpack_require__(28);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_7_common_PrerenderData_jsm__ = __webpack_require__(29);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_8_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_8_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_8_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_9_content_src_components_Search_Search__ = __webpack_require__(30);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_10_content_src_components_Sections_Sections__ = __webpack_require__(32);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_11_content_src_components_StartupOverlay_StartupOverlay__ = __webpack_require__(38);













const PrefsButton = Object(__WEBPACK_IMPORTED_MODULE_1_react_intl__["injectIntl"])(props => __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
  "div",
  { className: "prefs-button" },
  __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement("button", { className: "icon icon-settings", onClick: props.onClick, title: props.intl.formatMessage({ id: "settings_pane_button_label" }) })
));

// Add the locale data for pluralization and relative-time formatting for now,
// this just uses english locale data. We can make this more sophisticated if
// more features are needed.
function addLocaleDataForReactIntl(locale) {
  Object(__WEBPACK_IMPORTED_MODULE_1_react_intl__["addLocaleData"])([{ locale, parentLocale: "en" }]);
}

class _Base extends __WEBPACK_IMPORTED_MODULE_8_react___default.a.PureComponent {
  componentWillMount() {
    const { App, locale, Theme } = this.props;
    if (Theme.className) {
      this.updateTheme(Theme);
    }
    this.sendNewTabRehydrated(App);
    addLocaleDataForReactIntl(locale);
  }

  componentDidMount() {
    // Request state AFTER the first render to ensure we don't cause the
    // prerendered DOM to be unmounted. Otherwise, NEW_TAB_STATE_REQUEST is
    // dispatched right after the store is ready.
    if (this.props.isPrerendered) {
      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].NEW_TAB_STATE_REQUEST }));
      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].PAGE_PRERENDERED }));
    }
  }

  componentWillUnmount() {
    this.updateTheme({ className: "" });
  }

  componentWillUpdate({ App, Theme }) {
    this.updateTheme(Theme);
    this.sendNewTabRehydrated(App);
  }

  updateTheme(Theme) {
    const bodyClassName = ["activity-stream", Theme.className, this.props.isFirstrun ? "welcome" : ""].filter(v => v).join(" ");
    global.document.body.className = bodyClassName;
  }

  // The NEW_TAB_REHYDRATED event is used to inform feeds that their
  // data has been consumed e.g. for counting the number of tabs that
  // have rendered that data.
  sendNewTabRehydrated(App) {
    if (App && App.initialized && !this.renderNotified) {
      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].NEW_TAB_REHYDRATED, data: {} }));
      this.renderNotified = true;
    }
  }

  render() {
    const { props } = this;
    const { App, locale, strings } = props;
    const { initialized } = App;

    if (props.Prefs.values.asrouterExperimentEnabled && window.location.hash === "#asrouter") {
      return __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_content_src_components_ASRouterAdmin_ASRouterAdmin__["a" /* ASRouterAdmin */], null);
    }

    if (!props.isPrerendered && !initialized) {
      return null;
    }

    return __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
      __WEBPACK_IMPORTED_MODULE_1_react_intl__["IntlProvider"],
      { locale: locale, messages: strings },
      __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
        __WEBPACK_IMPORTED_MODULE_5_content_src_components_ErrorBoundary_ErrorBoundary__["a" /* ErrorBoundary */],
        { className: "base-content-fallback" },
        __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(BaseContent, this.props)
      )
    );
  }
}
/* unused harmony export _Base */


class BaseContent extends __WEBPACK_IMPORTED_MODULE_8_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
  }

  openPreferences() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].OnlyToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SETTINGS_OPEN }));
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: "OPEN_NEWTAB_PREFS" }));
  }

  render() {
    const { props } = this;
    const { App } = props;
    const { initialized } = App;
    const prefs = props.Prefs.values;

    const shouldBeFixedToTop = __WEBPACK_IMPORTED_MODULE_7_common_PrerenderData_jsm__["a" /* PrerenderData */].arePrefsValid(name => prefs[name]);

    const outerClassName = ["outer-wrapper", shouldBeFixedToTop && "fixed-to-top"].filter(v => v).join(" ");

    return __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
      "div",
      null,
      __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
        "div",
        { className: outerClassName },
        __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
          "main",
          null,
          prefs.showSearch && __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
            "div",
            { className: "non-collapsible-section" },
            __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
              __WEBPACK_IMPORTED_MODULE_5_content_src_components_ErrorBoundary_ErrorBoundary__["a" /* ErrorBoundary */],
              null,
              __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_9_content_src_components_Search_Search__["a" /* Search */], null)
            )
          ),
          __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
            "div",
            { className: `body-wrapper${initialized ? " on" : ""}` },
            !prefs.migrationExpired && __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(
              "div",
              { className: "non-collapsible-section" },
              __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_6_content_src_components_ManualMigration_ManualMigration__["a" /* ManualMigration */], null)
            ),
            __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_10_content_src_components_Sections_Sections__["a" /* Sections */], null),
            __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(PrefsButton, { onClick: this.openPreferences })
          ),
          __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_3_content_src_components_ConfirmDialog_ConfirmDialog__["a" /* ConfirmDialog */], null)
        )
      ),
      this.props.isFirstrun && __WEBPACK_IMPORTED_MODULE_8_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_11_content_src_components_StartupOverlay_StartupOverlay__["a" /* StartupOverlay */], null)
    );
  }
}
/* unused harmony export BaseContent */


const Base = Object(__WEBPACK_IMPORTED_MODULE_4_react_redux__["connect"])(state => ({ App: state.App, Prefs: state.Prefs, Theme: state.Theme }))(_Base);
/* harmony export (immutable) */ __webpack_exports__["a"] = Base;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 26 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__ = __webpack_require__(7);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react__);



class ASRouterAdmin extends __WEBPACK_IMPORTED_MODULE_1_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onMessage = this.onMessage.bind(this);
    this.findOtherBundledMessagesOfSameTemplate = this.findOtherBundledMessagesOfSameTemplate.bind(this);
    this.state = {};
  }

  onMessage({ data: action }) {
    if (action.type === "ADMIN_SET_STATE") {
      this.setState(action.data);
    }
  }

  componentWillMount() {
    __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].sendMessage({ type: "ADMIN_CONNECT_STATE" });
    __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].addListener(this.onMessage);
  }

  componentWillUnmount() {
    __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].removeListener(this.onMessage);
  }

  findOtherBundledMessagesOfSameTemplate(template) {
    return this.state.messages.filter(msg => msg.template === template && msg.bundled);
  }

  handleBlock(msg) {
    if (msg.bundled) {
      // If we are blocking a message that belongs to a bundle, block all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].blockBundle(bundle);
    }
    return () => __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].blockById(msg.id);
  }

  handleUnblock(msg) {
    if (msg.bundled) {
      // If we are unblocking a message that belongs to a bundle, unblock all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].unblockBundle(bundle);
    }
    return () => __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].unblockById(msg.id);
  }

  handleOverride(id) {
    return () => __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].overrideMessage(id);
  }

  renderMessageItem(msg) {
    const isCurrent = msg.id === this.state.currentId;
    const isBlocked = this.state.blockList.includes(msg.id);

    let itemClassName = "message-item";
    if (isCurrent) {
      itemClassName += " current";
    }
    if (isBlocked) {
      itemClassName += " blocked";
    }

    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "tr",
      { className: itemClassName, key: msg.id },
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "td",
        { className: "message-id" },
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "span",
          null,
          msg.id
        )
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "td",
        null,
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "button",
          { className: `button ${isBlocked ? "" : " primary"}`, onClick: isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg) },
          isBlocked ? "Unblock" : "Block"
        ),
        isBlocked ? null : __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "button",
          { className: "button", onClick: this.handleOverride(msg.id) },
          "Show"
        )
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "td",
        { className: "message-summary" },
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "pre",
          null,
          JSON.stringify(msg, null, 2)
        )
      )
    );
  }

  renderMessages() {
    if (!this.state.messages) {
      return null;
    }
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "table",
      null,
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "tbody",
        null,
        this.state.messages.map(msg => this.renderMessageItem(msg))
      )
    );
  }

  renderProviders() {
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "table",
      null,
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "tbody",
        null,
        this.state.providers.map((provider, i) => __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
          "tr",
          { className: "message-item", key: i },
          __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
            "td",
            null,
            provider.id
          ),
          __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
            "td",
            null,
            provider.type === "remote" ? __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
              "a",
              { target: "_blank", href: provider.url },
              provider.url
            ) : "(local)"
          )
        ))
      )
    );
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "div",
      { className: "asrouter-admin outer-wrapper" },
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "h1",
        null,
        "AS Router Admin"
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "button",
        { className: "button primary", onClick: __WEBPACK_IMPORTED_MODULE_0__asrouter_asrouter_content__["a" /* ASRouterUtils */].getNextMessage },
        "Refresh Current Message"
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "h2",
        null,
        "Message Providers"
      ),
      this.state.providers ? this.renderProviders() : null,
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "h2",
        null,
        "Messages"
      ),
      this.renderMessages()
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = ASRouterAdmin;


/***/ }),
/* 27 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react__);





/**
 * ConfirmDialog component.
 * One primary action button, one cancel button.
 *
 * Content displayed is controlled by `data` prop the component receives.
 * Example:
 * data: {
 *   // Any sort of data needed to be passed around by actions.
 *   payload: site.url,
 *   // Primary button AlsoToMain action.
 *   action: "DELETE_HISTORY_URL",
 *   // Primary button USerEvent action.
 *   userEvent: "DELETE",
 *   // Array of locale ids to display.
 *   message_body: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
 *   // Text for primary button.
 *   confirm_button_string_id: "menu_action_delete"
 * },
 */
class _ConfirmDialog extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DIALOG_CANCEL });
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].DIALOG_CANCEL, source: this.props.data.eventSource }));
  }

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  }

  _renderModalMessage() {
    const message_body = this.props.data.body_string_id;

    if (!message_body) {
      return null;
    }

    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "span",
      null,
      message_body.map(msg => __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "p",
        { key: msg },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: msg })
      ))
    );
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "div",
      { className: "confirmation-dialog" },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("div", { className: "modal-overlay", onClick: this._handleCancelBtn }),
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "div",
        { className: "modal" },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "section",
          { className: "modal-message" },
          this.props.data.icon && __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("span", { className: `icon icon-spacer icon-${this.props.data.icon}` }),
          this._renderModalMessage()
        ),
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "section",
          { className: "actions" },
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "button",
            { onClick: this._handleCancelBtn },
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: this.props.data.cancel_button_string_id })
          ),
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "button",
            { className: "done", onClick: this._handleConfirmBtn },
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: this.props.data.confirm_button_string_id })
          )
        )
      )
    );
  }
}
/* unused harmony export _ConfirmDialog */


const ConfirmDialog = Object(__WEBPACK_IMPORTED_MODULE_1_react_redux__["connect"])(state => state.Dialog)(_ConfirmDialog);
/* harmony export (immutable) */ __webpack_exports__["a"] = ConfirmDialog;


/***/ }),
/* 28 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react__);





/**
 * Manual migration component used to start the profile import wizard.
 * Message is presented temporarily and will go away if:
 * 1.  User clicks "No Thanks"
 * 2.  User completed the data import
 * 3.  After 3 active days
 * 4.  User clicks "Cancel" on the import wizard (currently not implemented).
 */
class _ManualMigration extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLaunchTour = this.onLaunchTour.bind(this);
    this.onCancelTour = this.onCancelTour.bind(this);
  }

  onLaunchTour() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].MIGRATION_START }));
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].MIGRATION_START }));
  }

  onCancelTour() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].MIGRATION_CANCEL }));
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].MIGRATION_CANCEL }));
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "div",
      { className: "manual-migration-container" },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "p",
        null,
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("span", { className: "icon icon-import" }),
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: "manual_migration_explanation2" })
      ),
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "div",
        { className: "manual-migration-actions actions" },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "button",
          { className: "dismiss", onClick: this.onCancelTour },
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: "manual_migration_cancel_button" })
        ),
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "button",
          { onClick: this.onLaunchTour },
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_2_react_intl__["FormattedMessage"], { id: "manual_migration_import_button" })
        )
      )
    );
  }
}
/* unused harmony export _ManualMigration */


const ManualMigration = Object(__WEBPACK_IMPORTED_MODULE_1_react_redux__["connect"])()(_ManualMigration);
/* harmony export (immutable) */ __webpack_exports__["a"] = ManualMigration;


/***/ }),
/* 29 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "a", function() { return PrerenderData; });
class _PrerenderData {
  constructor(options) {
    this.initialPrefs = options.initialPrefs;
    this.initialSections = options.initialSections;
    this._setValidation(options.validation);
  }

  get validation() {
    return this._validation;
  }

  set validation(value) {
    this._setValidation(value);
  }

  get invalidatingPrefs() {
    return this._invalidatingPrefs;
  }

  // This is needed so we can use it in the constructor
  _setValidation(value = []) {
    this._validation = value;
    this._invalidatingPrefs = value.reduce((result, next) => {
      if (typeof next === "string") {
        result.push(next);
        return result;
      } else if (next && next.oneOf) {
        return result.concat(next.oneOf);
      } else if (next && next.indexedDB) {
        return result.concat(next.indexedDB);
      }
      throw new Error("Your validation configuration is not properly configured");
    }, []);
  }

  arePrefsValid(getPref, indexedDBPrefs) {
    for (const prefs of this.validation) {
      // {oneOf: ["foo", "bar"]}
      if (prefs && prefs.oneOf && !prefs.oneOf.some(name => getPref(name) === this.initialPrefs[name])) {
        return false;

        // {indexedDB: ["foo", "bar"]}
      } else if (indexedDBPrefs && prefs && prefs.indexedDB) {
        const anyModifiedPrefs = prefs.indexedDB.some(prefName => indexedDBPrefs.some(pref => pref && pref[prefName]));
        if (anyModifiedPrefs) {
          return false;
        }
        // "foo"
      } else if (getPref(prefs) !== this.initialPrefs[prefs]) {
        return false;
      }
    }
    return true;
  }
}
/* unused harmony export _PrerenderData */

var PrerenderData = new _PrerenderData({
  initialPrefs: {
    "migrationExpired": true,
    "feeds.topsites": true,
    "showSearch": true,
    "topSitesRows": 1,
    "feeds.section.topstories": true,
    "feeds.section.highlights": true,
    "sectionOrder": "topsites,topstories,highlights",
    "collapsed": false
  },
  // Prefs listed as invalidating will prevent the prerendered version
  // of AS from being used if their value is something other than what is listed
  // here. This is required because some preferences cause the page layout to be
  // too different for the prerendered version to be used. Unfortunately, this
  // will result in users who have modified some of their preferences not being
  // able to get the benefits of prerendering.
  validation: ["feeds.topsites", "showSearch", "topSitesRows", "sectionOrder",
  // This means if either of these are set to their default values,
  // prerendering can be used.
  { oneOf: ["feeds.section.topstories", "feeds.section.highlights"] },
  // If any component has the following preference set to `true` it will
  // invalidate the prerendered version.
  { indexedDB: ["collapsed"] }],
  initialSections: [{
    enabled: true,
    icon: "pocket",
    id: "topstories",
    order: 1,
    title: { id: "header_recommended_by", values: { provider: "Pocket" } }
  }, {
    enabled: true,
    id: "highlights",
    icon: "highlights",
    order: 2,
    title: { id: "header_highlights" }
  }]
});

/***/ }),
/* 30 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_lib_constants__ = __webpack_require__(31);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_4_react__);
/* globals ContentSearchUIController */








class _Search extends __WEBPACK_IMPORTED_MODULE_4_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
  }

  handleEvent(event) {
    // Also track search events with our own telemetry
    if (event.detail.type === "Search") {
      this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: "SEARCH" }));
    }
  }

  onClick(event) {
    window.gContentSearchController.search(event);
  }

  componentWillUnmount() {
    delete window.gContentSearchController;
  }

  onInputMount(input) {
    if (input) {
      // The "healthReportKey" and needs to be "newtab" or "abouthome" so that
      // BrowserUsageTelemetry.jsm knows to handle events with this name, and
      // can add the appropriate telemetry probes for search. Without the correct
      // name, certain tests like browser_UsageTelemetry_content.js will fail
      // (See github ticket #2348 for more details)
      const healthReportKey = __WEBPACK_IMPORTED_MODULE_3_content_src_lib_constants__["a" /* IS_NEWTAB */] ? "newtab" : "abouthome";

      // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)
      const searchSource = __WEBPACK_IMPORTED_MODULE_3_content_src_lib_constants__["a" /* IS_NEWTAB */] ? "newtab" : "homepage";

      // gContentSearchController needs to exist as a global so that tests for
      // the existing about:home can find it; and so it allows these tests to pass.
      // In the future, when activity stream is default about:home, this can be renamed
      window.gContentSearchController = new ContentSearchUIController(input, input.parentNode, healthReportKey, searchSource);
      addEventListener("ContentSearchClient", this);
    } else {
      window.gContentSearchController = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  /*
   * Do not change the ID on the input field, as legacy newtab code
   * specifically looks for the id 'newtab-search-text' on input fields
   * in order to execute searches in various tests
   */
  render() {
    return __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
      "div",
      { className: "search-wrapper" },
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
        "label",
        { htmlFor: "newtab-search-text", className: "search-label" },
        __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
          "span",
          { className: "sr-only" },
          __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "search_web_placeholder" })
        )
      ),
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement("input", {
        id: "newtab-search-text",
        maxLength: "256",
        placeholder: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        ref: this.onInputMount,
        title: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        type: "search" }),
      __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
        "button",
        {
          id: "searchSubmit",
          className: "search-button",
          onClick: this.onClick,
          title: this.props.intl.formatMessage({ id: "search_button" }) },
        __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(
          "span",
          { className: "sr-only" },
          __WEBPACK_IMPORTED_MODULE_4_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "search_button" })
        )
      )
    );
  }
}
/* unused harmony export _Search */


const Search = Object(__WEBPACK_IMPORTED_MODULE_2_react_redux__["connect"])()(Object(__WEBPACK_IMPORTED_MODULE_0_react_intl__["injectIntl"])(_Search));
/* harmony export (immutable) */ __webpack_exports__["a"] = Search;


/***/ }),
/* 31 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {const IS_NEWTAB = global.document && global.document.documentURI === "about:newtab";
/* harmony export (immutable) */ __webpack_exports__["a"] = IS_NEWTAB;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 32 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_content_src_components_Card_Card__ = __webpack_require__(33);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_components_CollapsibleSection_CollapsibleSection__ = __webpack_require__(14);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_content_src_components_ComponentPerfTimer_ComponentPerfTimer__ = __webpack_require__(16);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_5_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_6_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_7_content_src_components_Topics_Topics__ = __webpack_require__(35);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_8_content_src_components_TopSites_TopSites__ = __webpack_require__(36);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };











const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const CARDS_PER_ROW = 3;

function getFormattedMessage(message) {
  return typeof message === "string" ? __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
    "span",
    null,
    message
  ) : __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_1_react_intl__["FormattedMessage"], message);
}

class Section extends __WEBPACK_IMPORTED_MODULE_6_react___default.a.PureComponent {
  _dispatchImpressionStats() {
    const { props } = this;
    const maxCards = 3 * props.maxRows;
    const cards = props.rows.slice(0, maxCards);

    if (this.needsImpressionStats(cards)) {
      props.dispatch(__WEBPACK_IMPORTED_MODULE_2_common_Actions_jsm__["b" /* actionCreators */].ImpressionStats({
        source: props.eventSource,
        tiles: cards.map(link => ({ id: link.guid }))
      }));
      this.impressionCardGuids = cards.map(link => link.guid);
    }
  }

  // This sends an event when a user sees a set of new content. If content
  // changes while the page is hidden (i.e. preloaded or on a hidden tab),
  // only send the event if the page becomes visible again.
  sendImpressionStatsOrAddListener() {
    const { props } = this;

    if (!props.shouldSendImpressionStats || !props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
      this._dispatchImpressionStats();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      // When the page becomes visible, send the impression stats ping if the section isn't collapsed.
      this._onVisibilityChange = () => {
        if (props.document.visibilityState === VISIBLE) {
          if (!this.props.pref.collapsed) {
            this._dispatchImpressionStats();
          }
          props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };
      props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  componentDidMount() {
    if (this.props.rows.length && !this.props.pref.collapsed) {
      this.sendImpressionStatsOrAddListener();
    }
  }

  componentDidUpdate(prevProps) {
    const { props } = this;
    const isCollapsed = props.pref.collapsed;
    const wasCollapsed = prevProps.pref.collapsed;
    if (
    // Don't send impression stats for the empty state
    props.rows.length && (
    // We only want to send impression stats if the content of the cards has changed
    // and the section is not collapsed...
    props.rows !== prevProps.rows && !isCollapsed ||
    // or if we are expanding a section that was collapsed.
    wasCollapsed && !isCollapsed)) {
      this.sendImpressionStatsOrAddListener();
    }
  }

  componentWillUnmount() {
    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  needsImpressionStats(cards) {
    if (!this.impressionCardGuids || this.impressionCardGuids.length !== cards.length) {
      return true;
    }

    for (let i = 0; i < cards.length; i++) {
      if (cards[i].guid !== this.impressionCardGuids[i]) {
        return true;
      }
    }

    return false;
  }

  numberOfPlaceholders(items) {
    if (items === 0) {
      return CARDS_PER_ROW;
    }
    const remainder = items % CARDS_PER_ROW;
    if (remainder === 0) {
      return 0;
    }
    return CARDS_PER_ROW - remainder;
  }

  render() {
    const {
      id, eventSource, title, icon, rows,
      emptyState, dispatch, maxRows,
      contextMenuOptions, initialized, disclaimer,
      pref, privacyNoticeURL, isFirst, isLast
    } = this.props;
    const maxCards = CARDS_PER_ROW * maxRows;

    // Show topics only for top stories and if it's not initialized yet (so
    // content doesn't shift when it is loaded) or has loaded with topics
    const shouldShowTopics = id === "topstories" && (!this.props.topics || this.props.topics.length > 0);

    const realRows = rows.slice(0, maxCards);
    const placeholders = this.numberOfPlaceholders(realRows.length);

    // The empty state should only be shown after we have initialized and there is no content.
    // Otherwise, we should show placeholders.
    const shouldShowEmptyState = initialized && !rows.length;

    // <Section> <-- React component
    // <section> <-- HTML5 element
    return __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
      __WEBPACK_IMPORTED_MODULE_4_content_src_components_ComponentPerfTimer_ComponentPerfTimer__["a" /* ComponentPerfTimer */],
      this.props,
      __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
        __WEBPACK_IMPORTED_MODULE_3_content_src_components_CollapsibleSection_CollapsibleSection__["a" /* CollapsibleSection */],
        { className: "section", icon: icon,
          title: title,
          id: id,
          eventSource: eventSource,
          disclaimer: disclaimer,
          collapsed: this.props.pref.collapsed,
          showPrefName: pref && pref.feed || id,
          privacyNoticeURL: privacyNoticeURL,
          Prefs: this.props.Prefs,
          isFirst: isFirst,
          isLast: isLast,
          dispatch: this.props.dispatch,
          isWebExtension: this.props.isWebExtension },
        !shouldShowEmptyState && __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
          "ul",
          { className: "section-list", style: { padding: 0 } },
          realRows.map((link, index) => link && __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_content_src_components_Card_Card__["a" /* Card */], { key: index, index: index, dispatch: dispatch, link: link, contextMenuOptions: contextMenuOptions,
            eventSource: eventSource, shouldSendImpressionStats: this.props.shouldSendImpressionStats, isWebExtension: this.props.isWebExtension })),
          placeholders > 0 && [...new Array(placeholders)].map((_, i) => __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_content_src_components_Card_Card__["b" /* PlaceholderCard */], { key: i }))
        ),
        shouldShowEmptyState && __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
          "div",
          { className: "section-empty-state" },
          __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
            "div",
            { className: "empty-state" },
            emptyState.icon && emptyState.icon.startsWith("moz-extension://") ? __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement("img", { className: "empty-state-icon icon", style: { "background-image": `url('${emptyState.icon}')` } }) : __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement("img", { className: `empty-state-icon icon icon-${emptyState.icon}` }),
            __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
              "p",
              { className: "empty-state-message" },
              getFormattedMessage(emptyState.message)
            )
          )
        ),
        shouldShowTopics && __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_7_content_src_components_Topics_Topics__["a" /* Topics */], { topics: this.props.topics, read_more_endpoint: this.props.read_more_endpoint })
      )
    );
  }
}
/* unused harmony export Section */


Section.defaultProps = {
  document: global.document,
  rows: [],
  emptyState: {},
  pref: {},
  title: ""
};

const SectionIntl = Object(__WEBPACK_IMPORTED_MODULE_5_react_redux__["connect"])(state => ({ Prefs: state.Prefs }))(Object(__WEBPACK_IMPORTED_MODULE_1_react_intl__["injectIntl"])(Section));
/* unused harmony export SectionIntl */


class _Sections extends __WEBPACK_IMPORTED_MODULE_6_react___default.a.PureComponent {
  renderSections() {
    const sections = [];
    const enabledSections = this.props.Sections.filter(section => section.enabled);
    const { sectionOrder, "feeds.topsites": showTopSites } = this.props.Prefs.values;
    // Enabled sections doesn't include Top Sites, so we add it if enabled.
    const expectedCount = enabledSections.length + ~~showTopSites;

    for (const sectionId of sectionOrder.split(",")) {
      const commonProps = {
        key: sectionId,
        isFirst: sections.length === 0,
        isLast: sections.length === expectedCount - 1
      };
      if (sectionId === "topsites" && showTopSites) {
        sections.push(__WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_8_content_src_components_TopSites_TopSites__["a" /* TopSites */], commonProps));
      } else {
        const section = enabledSections.find(s => s.id === sectionId);
        if (section) {
          sections.push(__WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(SectionIntl, _extends({}, section, commonProps)));
        }
      }
    }
    return sections;
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
      "div",
      { className: "sections-list" },
      this.renderSections()
    );
  }
}
/* unused harmony export _Sections */


const Sections = Object(__WEBPACK_IMPORTED_MODULE_5_react_redux__["connect"])(state => ({ Sections: state.Sections, Prefs: state.Prefs }))(_Sections);
/* harmony export (immutable) */ __webpack_exports__["a"] = Sections;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 33 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./system-addon/common/Actions.jsm
var Actions = __webpack_require__(1);

// CONCATENATED MODULE: ./system-addon/content-src/components/Card/types.js
const cardContextTypes = {
  history: {
    intlID: "type_label_visited",
    icon: "historyItem"
  },
  bookmark: {
    intlID: "type_label_bookmarked",
    icon: "bookmark-added"
  },
  trending: {
    intlID: "type_label_recommended",
    icon: "trending"
  },
  now: {
    intlID: "type_label_now",
    icon: "now"
  },
  pocket: {
    intlID: "type_label_pocket",
    icon: "pocket"
  },
  download: {
    intlID: "type_label_downloaded",
    icon: "download"
  }
};
// EXTERNAL MODULE: external "ReactRedux"
var external__ReactRedux_ = __webpack_require__(4);
var external__ReactRedux__default = /*#__PURE__*/__webpack_require__.n(external__ReactRedux_);

// EXTERNAL MODULE: external "ReactIntl"
var external__ReactIntl_ = __webpack_require__(2);
var external__ReactIntl__default = /*#__PURE__*/__webpack_require__.n(external__ReactIntl_);

// EXTERNAL MODULE: ./system-addon/content-src/lib/link-menu-options.js
var link_menu_options = __webpack_require__(11);

// EXTERNAL MODULE: ./system-addon/content-src/components/LinkMenu/LinkMenu.jsx
var LinkMenu = __webpack_require__(12);

// EXTERNAL MODULE: external "React"
var external__React_ = __webpack_require__(0);
var external__React__default = /*#__PURE__*/__webpack_require__.n(external__React_);

// CONCATENATED MODULE: ./system-addon/content-src/components/Card/Card.jsx








// Keep track of pending image loads to only request once
const gImageLoading = new Map();

/**
 * Card component.
 * Cards are found within a Section component and contain information about a link such
 * as preview image, page title, page description, and some context about if the page
 * was visited, bookmarked, trending etc...
 * Each Section can make an unordered list of Cards which will create one instane of
 * this class. Each card will then get a context menu which reflects the actions that
 * can be done on this Card.
 */
class Card__Card extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      activeCard: null,
      imageLoaded: false,
      showContextMenu: false
    };
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  /**
   * Helper to conditionally load an image and update state when it loads.
   */
  async maybeLoadImage() {
    // No need to load if it's already loaded or no image
    const { image } = this.props.link;
    if (!this.state.imageLoaded && image) {
      // Initialize a promise to share a load across multiple card updates
      if (!gImageLoading.has(image)) {
        const loaderPromise = new Promise((resolve, reject) => {
          const loader = new Image();
          loader.addEventListener("load", resolve);
          loader.addEventListener("error", reject);
          loader.src = image;
        });

        // Save and remove the promise only while it's pending
        gImageLoading.set(image, loaderPromise);
        loaderPromise.catch(ex => ex).then(() => gImageLoading.delete(image)).catch();
      }

      // Wait for the image whether just started loading or reused promise
      await gImageLoading.get(image);

      // Only update state if we're still waiting to load the original image
      if (this.props.link.image === image && !this.state.imageLoaded) {
        this.setState({ imageLoaded: true });
      }
    }
  }

  onMenuButtonClick(event) {
    event.preventDefault();
    this.setState({
      activeCard: this.props.index,
      showContextMenu: true
    });
  }

  /**
   * Report to telemetry additional information about the item.
   */
  _getTelemetryInfo() {
    // Filter out "history" type for being the default
    if (this.props.link.type !== "history") {
      return { value: { card_type: this.props.link.type } };
    }

    return null;
  }

  onLinkClick(event) {
    event.preventDefault();
    if (this.props.link.type === "download") {
      this.props.dispatch(Actions["b" /* actionCreators */].OnlyToMain({
        type: Actions["c" /* actionTypes */].SHOW_DOWNLOAD_FILE,
        data: this.props.link
      }));
    } else {
      const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
      this.props.dispatch(Actions["b" /* actionCreators */].OnlyToMain({
        type: Actions["c" /* actionTypes */].OPEN_LINK,
        data: Object.assign(this.props.link, { event: { altKey, button, ctrlKey, metaKey, shiftKey } })
      }));
    }
    if (this.props.isWebExtension) {
      this.props.dispatch(Actions["b" /* actionCreators */].WebExtEvent(Actions["c" /* actionTypes */].WEBEXT_CLICK, {
        source: this.props.eventSource,
        url: this.props.link.url,
        action_position: this.props.index
      }));
    } else {
      this.props.dispatch(Actions["b" /* actionCreators */].UserEvent(Object.assign({
        event: "CLICK",
        source: this.props.eventSource,
        action_position: this.props.index
      }, this._getTelemetryInfo())));

      if (this.props.shouldSendImpressionStats) {
        this.props.dispatch(Actions["b" /* actionCreators */].ImpressionStats({
          source: this.props.eventSource,
          click: 0,
          tiles: [{ id: this.props.link.guid, pos: this.props.index }]
        }));
      }
    }
  }

  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }

  componentDidMount() {
    this.maybeLoadImage();
  }

  componentDidUpdate() {
    this.maybeLoadImage();
  }

  componentWillReceiveProps(nextProps) {
    // Clear the image state if changing images
    if (nextProps.link.image !== this.props.link.image) {
      this.setState({ imageLoaded: false });
    }
  }

  render() {
    const { index, link, dispatch, contextMenuOptions, eventSource, shouldSendImpressionStats } = this.props;
    const { props } = this;
    const isContextMenuOpen = this.state.showContextMenu && this.state.activeCard === index;
    // Display "now" as "trending" until we have new strings #3402
    const { icon, intlID } = cardContextTypes[link.type === "now" ? "trending" : link.type] || {};
    const hasImage = link.image || link.hasImage;
    const imageStyle = { backgroundImage: link.image ? `url(${link.image})` : "none" };

    return external__React__default.a.createElement(
      "li",
      { className: `card-outer${isContextMenuOpen ? " active" : ""}${props.placeholder ? " placeholder" : ""}` },
      external__React__default.a.createElement(
        "a",
        { href: link.type === "pocket" ? link.open_url : link.url, onClick: !props.placeholder ? this.onLinkClick : undefined },
        external__React__default.a.createElement(
          "div",
          { className: "card" },
          hasImage && external__React__default.a.createElement(
            "div",
            { className: "card-preview-image-outer" },
            external__React__default.a.createElement("div", { className: `card-preview-image${this.state.imageLoaded ? " loaded" : ""}`, style: imageStyle })
          ),
          external__React__default.a.createElement(
            "div",
            { className: `card-details${hasImage ? "" : " no-image"}` },
            link.type === "download" && external__React__default.a.createElement("div", { className: "card-download-icon icon icon-download-folder" }),
            link.type === "download" && external__React__default.a.createElement(
              "div",
              { className: "card-host-name alternate" },
              external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: Object(link_menu_options["a" /* GetPlatformString */])(this.props.platform) })
            ),
            link.hostname && external__React__default.a.createElement(
              "div",
              { className: "card-host-name" },
              link.hostname
            ),
            external__React__default.a.createElement(
              "div",
              { className: ["card-text", icon ? "" : "no-context", link.description ? "" : "no-description", link.hostname ? "" : "no-host-name", hasImage ? "" : "no-image"].join(" ") },
              external__React__default.a.createElement(
                "h4",
                { className: "card-title", dir: "auto" },
                link.title
              ),
              external__React__default.a.createElement(
                "p",
                { className: "card-description", dir: "auto" },
                link.description
              )
            ),
            external__React__default.a.createElement(
              "div",
              { className: "card-context" },
              icon && !link.context && external__React__default.a.createElement("span", { className: `card-context-icon icon icon-${icon}` }),
              link.icon && link.context && external__React__default.a.createElement("span", { className: "card-context-icon icon", style: { backgroundImage: `url('${link.icon}')` } }),
              intlID && !link.context && external__React__default.a.createElement(
                "div",
                { className: "card-context-label" },
                external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: intlID, defaultMessage: "Visited" })
              ),
              link.context && external__React__default.a.createElement(
                "div",
                { className: "card-context-label" },
                link.context
              )
            )
          )
        )
      ),
      !props.placeholder && external__React__default.a.createElement(
        "button",
        { className: "context-menu-button icon",
          onClick: this.onMenuButtonClick },
        external__React__default.a.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${link.title}`
        )
      ),
      isContextMenuOpen && external__React__default.a.createElement(LinkMenu["a" /* LinkMenu */], {
        dispatch: dispatch,
        index: index,
        source: eventSource,
        onUpdate: this.onMenuUpdate,
        options: link.contextMenuOptions || contextMenuOptions,
        site: link,
        siteInfo: this._getTelemetryInfo(),
        shouldSendImpressionStats: shouldSendImpressionStats })
    );
  }
}
/* unused harmony export _Card */

Card__Card.defaultProps = { link: {} };
const Card = Object(external__ReactRedux_["connect"])(state => ({ platform: state.Prefs.values.platform }))(Card__Card);
/* harmony export (immutable) */ __webpack_exports__["a"] = Card;

const PlaceholderCard = () => external__React__default.a.createElement(Card, { placeholder: true });
/* harmony export (immutable) */ __webpack_exports__["b"] = PlaceholderCard;


/***/ }),
/* 34 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_content_src_components_ContextMenu_ContextMenu__ = __webpack_require__(13);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_content_src_lib_section_menu_options__ = __webpack_require__(15);






const DEFAULT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "RemoveSection", "CheckCollapsed", "Separator", "ManageSection"];
const WEBEXT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "CheckCollapsed", "Separator", "ManageWebExtension"];

class _SectionMenu extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  getOptions() {
    const { props } = this;

    const propOptions = props.isWebExtension ? [...WEBEXT_SECTION_MENU_OPTIONS] : [...DEFAULT_SECTION_MENU_OPTIONS];
    // Prepend custom options and a separator
    if (props.extraOptions) {
      propOptions.splice(0, 0, ...props.extraOptions, "Separator");
    }
    // Insert privacy notice before the last option ("ManageSection")
    if (props.privacyNoticeURL) {
      propOptions.splice(-1, 0, "PrivacyNotice");
    }

    const options = propOptions.map(o => __WEBPACK_IMPORTED_MODULE_4_content_src_lib_section_menu_options__["a" /* SectionMenuOptions */][o](props)).map(option => {
      const { action, id, type, userEvent } = option;
      if (!type && id) {
        option.label = props.intl.formatMessage({ id });
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({
              event: userEvent,
              source: props.source
            }));
          }
        };
      }
      return option;
    });

    // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.
    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_1_content_src_components_ContextMenu_ContextMenu__["a" /* ContextMenu */], {
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}
/* unused harmony export _SectionMenu */


const SectionMenu = Object(__WEBPACK_IMPORTED_MODULE_2_react_intl__["injectIntl"])(_SectionMenu);
/* harmony export (immutable) */ __webpack_exports__["a"] = SectionMenu;


/***/ }),
/* 35 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_1_react__);



class Topic extends __WEBPACK_IMPORTED_MODULE_1_react___default.a.PureComponent {
  render() {
    const { url, name } = this.props;
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "li",
      null,
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "a",
        { key: name, className: "topic-link", href: url },
        name
      )
    );
  }
}
/* unused harmony export Topic */


class Topics extends __WEBPACK_IMPORTED_MODULE_1_react___default.a.PureComponent {
  render() {
    const { topics, read_more_endpoint } = this.props;
    return __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
      "div",
      { className: "topic" },
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "span",
        null,
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "pocket_read_more" })
      ),
      __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "ul",
        null,
        topics && topics.map(t => __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(Topic, { key: t.name, url: t.url, name: t.name }))
      ),
      read_more_endpoint && __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(
        "a",
        { className: "topic-read-more", href: read_more_endpoint },
        __WEBPACK_IMPORTED_MODULE_1_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "pocket_read_even_more" })
      )
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = Topics;


/***/ }),
/* 36 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1__TopSitesConstants__ = __webpack_require__(5);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_content_src_components_CollapsibleSection_CollapsibleSection__ = __webpack_require__(14);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_content_src_components_ComponentPerfTimer_ComponentPerfTimer__ = __webpack_require__(16);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_4_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_4_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_5_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_5_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_6_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_6_react__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_7_common_Reducers_jsm__ = __webpack_require__(6);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_8__TopSiteForm__ = __webpack_require__(37);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_9__TopSite__ = __webpack_require__(18);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };












function topSiteIconType(link) {
  if (link.customScreenshotURL) {
    return "custom_screenshot";
  }
  if (link.tippyTopIcon || link.faviconRef === "tippytop") {
    return "tippytop";
  }
  if (link.faviconSize >= __WEBPACK_IMPORTED_MODULE_1__TopSitesConstants__["b" /* MIN_RICH_FAVICON_SIZE */]) {
    return "rich_icon";
  }
  if (link.screenshot && link.faviconSize >= __WEBPACK_IMPORTED_MODULE_1__TopSitesConstants__["a" /* MIN_CORNER_FAVICON_SIZE */]) {
    return "screenshot_with_icon";
  }
  if (link.screenshot) {
    return "screenshot";
  }
  return "no_image";
}

/**
 * Iterates through TopSites and counts types of images.
 * @param acc Accumulator for reducer.
 * @param topsite Entry in TopSites.
 */
function countTopSitesIconsTypes(topSites) {
  const countTopSitesTypes = (acc, link) => {
    acc[topSiteIconType(link)]++;
    return acc;
  };

  return topSites.reduce(countTopSitesTypes, {
    "custom_screenshot": 0,
    "screenshot_with_icon": 0,
    "screenshot": 0,
    "tippytop": 0,
    "rich_icon": 0,
    "no_image": 0
  });
}

class _TopSites extends __WEBPACK_IMPORTED_MODULE_6_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onFormClose = this.onFormClose.bind(this);
  }

  /**
   * Dispatch session statistics about the quality of TopSites icons and pinned count.
   */
  _dispatchTopSitesStats() {
    const topSites = this._getVisibleTopSites();
    const topSitesIconsStats = countTopSitesIconsTypes(topSites);
    const topSitesPinned = topSites.filter(site => !!site.isPinned).length;
    // Dispatch telemetry event with the count of TopSites images types.
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
      type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_SESSION_PERF_DATA,
      data: { topsites_icon_stats: topSitesIconsStats, topsites_pinned: topSitesPinned }
    }));
  }

  /**
   * Return the TopSites that are visible based on prefs and window width.
   */
  _getVisibleTopSites() {
    // We hide 2 sites per row when not in the wide layout.
    let sitesPerRow = __WEBPACK_IMPORTED_MODULE_7_common_Reducers_jsm__["a" /* TOP_SITES_MAX_SITES_PER_ROW */];
    // $break-point-widest = 1072px (from _variables.scss)
    if (!global.matchMedia(`(min-width: 1072px)`).matches) {
      sitesPerRow -= 2;
    }
    return this.props.TopSites.rows.slice(0, this.props.TopSitesRows * sitesPerRow);
  }

  componentDidUpdate() {
    this._dispatchTopSitesStats();
  }

  componentDidMount() {
    this._dispatchTopSitesStats();
  }

  onFormClose() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].UserEvent({
      source: __WEBPACK_IMPORTED_MODULE_1__TopSitesConstants__["d" /* TOP_SITES_SOURCE */],
      event: "TOP_SITES_EDIT_CLOSE"
    }));
    this.props.dispatch({ type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].TOP_SITES_CANCEL_EDIT });
  }

  render() {
    const { props } = this;
    const { editForm } = props.TopSites;

    return __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
      __WEBPACK_IMPORTED_MODULE_3_content_src_components_ComponentPerfTimer_ComponentPerfTimer__["a" /* ComponentPerfTimer */],
      { id: "topsites", initialized: props.TopSites.initialized, dispatch: props.dispatch },
      __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
        __WEBPACK_IMPORTED_MODULE_2_content_src_components_CollapsibleSection_CollapsibleSection__["a" /* CollapsibleSection */],
        {
          className: "top-sites",
          icon: "topsites",
          id: "topsites",
          title: { id: "header_top_sites" },
          extraMenuOptions: ["AddTopSite"],
          showPrefName: "feeds.topsites",
          eventSource: __WEBPACK_IMPORTED_MODULE_1__TopSitesConstants__["d" /* TOP_SITES_SOURCE */],
          collapsed: props.TopSites.pref ? props.TopSites.pref.collapsed : undefined,
          isFirst: props.isFirst,
          isLast: props.isLast,
          dispatch: props.dispatch },
        __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_9__TopSite__["b" /* TopSiteList */], { TopSites: props.TopSites, TopSitesRows: props.TopSitesRows, dispatch: props.dispatch, intl: props.intl, topSiteIconType: topSiteIconType }),
        __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
          "div",
          { className: "edit-topsites-wrapper" },
          editForm && __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
            "div",
            { className: "edit-topsites" },
            __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement("div", { className: "modal-overlay", onClick: this.onFormClose }),
            __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(
              "div",
              { className: "modal" },
              __WEBPACK_IMPORTED_MODULE_6_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_8__TopSiteForm__["a" /* TopSiteForm */], _extends({
                site: props.TopSites.rows[editForm.index],
                onClose: this.onFormClose,
                dispatch: this.props.dispatch,
                intl: this.props.intl
              }, editForm))
            )
          )
        )
      )
    );
  }
}
/* unused harmony export _TopSites */


const TopSites = Object(__WEBPACK_IMPORTED_MODULE_4_react_redux__["connect"])(state => ({
  TopSites: state.TopSites,
  Prefs: state.Prefs,
  TopSitesRows: state.Prefs.values.topSitesRows
}))(Object(__WEBPACK_IMPORTED_MODULE_5_react_intl__["injectIntl"])(_TopSites));
/* harmony export (immutable) */ __webpack_exports__["a"] = TopSites;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ }),
/* 37 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./system-addon/common/Actions.jsm
var Actions = __webpack_require__(1);

// EXTERNAL MODULE: external "ReactIntl"
var external__ReactIntl_ = __webpack_require__(2);
var external__ReactIntl__default = /*#__PURE__*/__webpack_require__.n(external__ReactIntl_);

// EXTERNAL MODULE: external "React"
var external__React_ = __webpack_require__(0);
var external__React__default = /*#__PURE__*/__webpack_require__.n(external__React_);

// EXTERNAL MODULE: ./system-addon/content-src/components/TopSites/TopSitesConstants.js
var TopSitesConstants = __webpack_require__(5);

// CONCATENATED MODULE: ./system-addon/content-src/components/TopSites/TopSiteFormInput.jsx



class TopSiteFormInput_TopSiteFormInput extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = { validationError: this.props.validationError };
    this.onChange = this.onChange.bind(this);
    this.onMount = this.onMount.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (nextProps.shouldFocus && !this.props.shouldFocus) {
      this.input.focus();
    }
    if (nextProps.validationError && !this.props.validationError) {
      this.setState({ validationError: true });
    }
    // If the component is in an error state but the value was cleared by the parent
    if (this.state.validationError && !nextProps.value) {
      this.setState({ validationError: false });
    }
  }

  onChange(ev) {
    if (this.state.validationError) {
      this.setState({ validationError: false });
    }
    this.props.onChange(ev);
  }

  onMount(input) {
    this.input = input;
  }

  render() {
    const showClearButton = this.props.value && this.props.onClear;
    const { typeUrl } = this.props;
    const { validationError } = this.state;

    return external__React__default.a.createElement(
      "label",
      null,
      external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: this.props.titleId }),
      external__React__default.a.createElement(
        "div",
        { className: `field ${typeUrl ? "url" : ""}${validationError ? " invalid" : ""}` },
        this.props.loading ? external__React__default.a.createElement(
          "div",
          { className: "loading-container" },
          external__React__default.a.createElement("div", { className: "loading-animation" })
        ) : showClearButton && external__React__default.a.createElement("div", { className: "icon icon-clear-input", onClick: this.props.onClear }),
        external__React__default.a.createElement("input", { type: "text",
          value: this.props.value,
          ref: this.onMount,
          onChange: this.onChange,
          placeholder: this.props.intl.formatMessage({ id: this.props.placeholderId }),
          autoFocus: this.props.shouldFocus,
          disabled: this.props.loading }),
        validationError && external__React__default.a.createElement(
          "aside",
          { className: "error-tooltip" },
          external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: this.props.errorMessageId })
        )
      )
    );
  }
}

TopSiteFormInput_TopSiteFormInput.defaultProps = {
  showClearButton: false,
  value: "",
  validationError: false
};
// EXTERNAL MODULE: ./system-addon/content-src/components/TopSites/TopSite.jsx
var TopSite = __webpack_require__(18);

// CONCATENATED MODULE: ./system-addon/content-src/components/TopSites/TopSiteForm.jsx







class TopSiteForm_TopSiteForm extends external__React__default.a.PureComponent {
  constructor(props) {
    super(props);
    const { site } = props;
    this.state = {
      label: site ? site.label || site.hostname : "",
      url: site ? site.url : "",
      validationError: false,
      customScreenshotUrl: site ? site.customScreenshotURL : "",
      showCustomScreenshotForm: site ? site.customScreenshotURL : false
    };
    this.onClearScreenshotInput = this.onClearScreenshotInput.bind(this);
    this.onLabelChange = this.onLabelChange.bind(this);
    this.onUrlChange = this.onUrlChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onClearUrlClick = this.onClearUrlClick.bind(this);
    this.onDoneButtonClick = this.onDoneButtonClick.bind(this);
    this.onCustomScreenshotUrlChange = this.onCustomScreenshotUrlChange.bind(this);
    this.onPreviewButtonClick = this.onPreviewButtonClick.bind(this);
    this.onEnableScreenshotUrlForm = this.onEnableScreenshotUrlForm.bind(this);
    this.validateUrl = this.validateUrl.bind(this);
  }

  onLabelChange(event) {
    this.setState({ "label": event.target.value });
  }

  onUrlChange(event) {
    this.setState({
      url: event.target.value,
      validationError: false
    });
  }

  onClearUrlClick() {
    this.setState({
      url: "",
      validationError: false
    });
  }

  onEnableScreenshotUrlForm() {
    this.setState({ showCustomScreenshotForm: true });
  }

  _updateCustomScreenshotInput(customScreenshotUrl) {
    this.setState({
      customScreenshotUrl,
      validationError: false
    });
    this.props.dispatch({ type: Actions["c" /* actionTypes */].PREVIEW_REQUEST_CANCEL });
  }

  onCustomScreenshotUrlChange(event) {
    this._updateCustomScreenshotInput(event.target.value);
  }

  onClearScreenshotInput() {
    this._updateCustomScreenshotInput("");
  }

  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }

  onDoneButtonClick(ev) {
    ev.preventDefault();

    if (this.validateForm()) {
      const site = { url: this.cleanUrl(this.state.url) };
      const { index } = this.props;
      if (this.state.label !== "") {
        site.label = this.state.label;
      }

      if (this.state.customScreenshotUrl) {
        site.customScreenshotURL = this.cleanUrl(this.state.customScreenshotUrl);
      } else if (this.props.site && this.props.site.customScreenshotURL) {
        // Used to flag that previously cached screenshot should be removed
        site.customScreenshotURL = null;
      }
      this.props.dispatch(Actions["b" /* actionCreators */].AlsoToMain({
        type: Actions["c" /* actionTypes */].TOP_SITES_PIN,
        data: { site, index }
      }));
      this.props.dispatch(Actions["b" /* actionCreators */].UserEvent({
        source: TopSitesConstants["d" /* TOP_SITES_SOURCE */],
        event: "TOP_SITES_EDIT",
        action_position: index
      }));

      this.props.onClose();
    }
  }

  onPreviewButtonClick(event) {
    event.preventDefault();
    if (this.validateForm()) {
      this.props.dispatch(Actions["b" /* actionCreators */].AlsoToMain({
        type: Actions["c" /* actionTypes */].PREVIEW_REQUEST,
        data: { url: this.cleanUrl(this.state.customScreenshotUrl) }
      }));
      this.props.dispatch(Actions["b" /* actionCreators */].UserEvent({
        source: TopSitesConstants["d" /* TOP_SITES_SOURCE */],
        event: "PREVIEW_REQUEST"
      }));
    }
  }

  cleanUrl(url) {
    // If we are missing a protocol, prepend http://
    if (!url.startsWith("http:") && !url.startsWith("https:")) {
      return `http://${url}`;
    }
    return url;
  }

  _tryParseUrl(url) {
    try {
      return new URL(url);
    } catch (e) {
      return null;
    }
  }

  validateUrl(url) {
    const validProtocols = ["http:", "https:"];
    const urlObj = this._tryParseUrl(url) || this._tryParseUrl(this.cleanUrl(url));

    return urlObj && validProtocols.includes(urlObj.protocol);
  }

  validateCustomScreenshotUrl() {
    const { customScreenshotUrl } = this.state;
    return !customScreenshotUrl || this.validateUrl(customScreenshotUrl);
  }

  validateForm() {
    const validate = this.validateUrl(this.state.url) && this.validateCustomScreenshotUrl();

    if (!validate) {
      this.setState({ validationError: true });
    }

    return validate;
  }

  _renderCustomScreenshotInput() {
    const { customScreenshotUrl } = this.state;
    const requestFailed = this.props.previewResponse === "";
    const validationError = this.state.validationError && !this.validateCustomScreenshotUrl() || requestFailed;
    // Set focus on error if the url field is valid or when the input is first rendered and is empty
    const shouldFocus = validationError && this.validateUrl(this.state.url) || !customScreenshotUrl;
    const isLoading = this.props.previewResponse === null && customScreenshotUrl && this.props.previewUrl === this.cleanUrl(customScreenshotUrl);

    if (!this.state.showCustomScreenshotForm) {
      return external__React__default.a.createElement(
        "a",
        { className: "enable-custom-image-input", onClick: this.onEnableScreenshotUrlForm },
        external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: "topsites_form_use_image_link" })
      );
    }
    return external__React__default.a.createElement(
      "div",
      { className: "custom-image-input-container" },
      external__React__default.a.createElement(TopSiteFormInput_TopSiteFormInput, {
        errorMessageId: requestFailed ? "topsites_form_image_validation" : "topsites_form_url_validation",
        loading: isLoading,
        onChange: this.onCustomScreenshotUrlChange,
        onClear: this.onClearScreenshotInput,
        shouldFocus: shouldFocus,
        typeUrl: true,
        value: customScreenshotUrl,
        validationError: validationError,
        titleId: "topsites_form_image_url_label",
        placeholderId: "topsites_form_url_placeholder",
        intl: this.props.intl })
    );
  }

  render() {
    const { customScreenshotUrl } = this.state;
    const requestFailed = this.props.previewResponse === "";
    // For UI purposes, editing without an existing link is "add"
    const showAsAdd = !this.props.site;
    const previous = this.props.site && this.props.site.customScreenshotURL || "";
    const changed = customScreenshotUrl && this.cleanUrl(customScreenshotUrl) !== previous;
    // Preview mode if changes were made to the custom screenshot URL and no preview was received yet
    // or the request failed
    const previewMode = changed && !this.props.previewResponse;
    const previewLink = Object.assign({}, this.props.site);
    if (this.props.previewResponse) {
      previewLink.screenshot = this.props.previewResponse;
      previewLink.customScreenshotURL = this.props.previewUrl;
    }
    return external__React__default.a.createElement(
      "form",
      { className: "topsite-form" },
      external__React__default.a.createElement(
        "div",
        { className: "form-input-container" },
        external__React__default.a.createElement(
          "h3",
          { className: "section-title" },
          external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: showAsAdd ? "topsites_form_add_header" : "topsites_form_edit_header" })
        ),
        external__React__default.a.createElement(
          "div",
          { className: "fields-and-preview" },
          external__React__default.a.createElement(
            "div",
            { className: "form-wrapper" },
            external__React__default.a.createElement(TopSiteFormInput_TopSiteFormInput, { onChange: this.onLabelChange,
              value: this.state.label,
              titleId: "topsites_form_title_label",
              placeholderId: "topsites_form_title_placeholder",
              intl: this.props.intl }),
            external__React__default.a.createElement(TopSiteFormInput_TopSiteFormInput, { onChange: this.onUrlChange,
              shouldFocus: this.state.validationError && !this.validateUrl(this.state.url),
              value: this.state.url,
              onClear: this.onClearUrlClick,
              validationError: this.state.validationError && !this.validateUrl(this.state.url),
              titleId: "topsites_form_url_label",
              typeUrl: true,
              placeholderId: "topsites_form_url_placeholder",
              errorMessageId: "topsites_form_url_validation",
              intl: this.props.intl }),
            this._renderCustomScreenshotInput()
          ),
          external__React__default.a.createElement(TopSite["a" /* TopSiteLink */], { link: previewLink,
            defaultStyle: requestFailed,
            title: this.state.label })
        )
      ),
      external__React__default.a.createElement(
        "section",
        { className: "actions" },
        external__React__default.a.createElement(
          "button",
          { className: "cancel", type: "button", onClick: this.onCancelButtonClick },
          external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: "topsites_form_cancel_button" })
        ),
        previewMode ? external__React__default.a.createElement(
          "button",
          { className: "done preview", type: "submit", onClick: this.onPreviewButtonClick },
          external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: "topsites_form_preview_button" })
        ) : external__React__default.a.createElement(
          "button",
          { className: "done", type: "submit", onClick: this.onDoneButtonClick },
          external__React__default.a.createElement(external__ReactIntl_["FormattedMessage"], { id: showAsAdd ? "topsites_form_add_button" : "topsites_form_save_button" })
        )
      )
    );
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = TopSiteForm_TopSiteForm;


TopSiteForm_TopSiteForm.defaultProps = {
  site: null,
  index: -1
};

/***/ }),
/* 38 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl__ = __webpack_require__(2);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_react_intl___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_0_react_intl__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_redux__ = __webpack_require__(4);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_2_react_redux___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_2_react_redux__);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react__ = __webpack_require__(0);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_3_react___default = __webpack_require__.n(__WEBPACK_IMPORTED_MODULE_3_react__);





class _StartupOverlay extends __WEBPACK_IMPORTED_MODULE_3_react___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onInputChange = this.onInputChange.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.clickSkip = this.clickSkip.bind(this);
    this.initScene = this.initScene.bind(this);
    this.removeOverlay = this.removeOverlay.bind(this);

    this.state = { emailInput: "" };
    this.initScene();
  }

  initScene() {
    // Timeout to allow the scene to render once before attaching the attribute
    // to trigger the animation.
    setTimeout(() => {
      this.setState({ show: true });
    }, 10);
  }

  removeOverlay() {
    window.removeEventListener("visibilitychange", this.removeOverlay);
    this.setState({ show: false });
    setTimeout(() => {
      // Allow scrolling and fully remove overlay after animation finishes.
      document.body.classList.remove("welcome");
    }, 400);
  }

  onInputChange(e) {
    this.setState({ emailInput: e.target.value });
  }

  onSubmit() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: "SUBMIT_EMAIL" }));
    window.addEventListener("visibilitychange", this.removeOverlay);
  }

  clickSkip() {
    this.props.dispatch(__WEBPACK_IMPORTED_MODULE_1_common_Actions_jsm__["b" /* actionCreators */].UserEvent({ event: "SKIPPED_SIGNIN" }));
    this.removeOverlay();
  }

  render() {
    let termsLink = __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "a",
      { href: "https://accounts.firefox.com/legal/terms", target: "_blank", rel: "noopener noreferrer" },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_terms_of_service" })
    );
    let privacyLink = __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "a",
      { href: "https://accounts.firefox.com/legal/privacy", target: "_blank", rel: "noopener noreferrer" },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_privacy_notice" })
    );
    return __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
      "div",
      { className: `overlay-wrapper ${this.state.show ? "show " : ""}` },
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("div", { className: "background" }),
      __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
        "div",
        { className: "firstrun-scene" },
        __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
          "div",
          { className: "fxaccounts-container" },
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "div",
            { className: "firstrun-left-divider" },
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "h1",
              { className: "firstrun-title" },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_title" })
            ),
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "p",
              { className: "firstrun-content" },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_content" })
            ),
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "a",
              { className: "firstrun-link", href: "https://www.mozilla.org/firefox/features/sync/", target: "_blank", rel: "noopener noreferrer" },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_learn_more_link" })
            )
          ),
          __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
            "div",
            { className: "firstrun-sign-in" },
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "p",
              { className: "form-header" },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_form_header" }),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
                "span",
                null,
                __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_form_sub_header" })
              )
            ),
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "form",
              { method: "get", action: "https://accounts.firefox.com", target: "_blank", rel: "noopener noreferrer", onSubmit: this.onSubmit },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("input", { name: "service", type: "hidden", value: "sync" }),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("input", { name: "action", type: "hidden", value: "email" }),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("input", { name: "context", type: "hidden", value: "fx_desktop_v3" }),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement("input", { className: "email-input", name: "email", type: "email", required: "true", placeholder: this.props.intl.formatMessage({ id: "firstrun_email_input_placeholder" }), onChange: this.onInputChange }),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
                "div",
                { className: "extra-links" },
                __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], {
                  id: "firstrun_extra_legal_links",
                  values: {
                    terms: termsLink,
                    privacy: privacyLink
                  } })
              ),
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
                "button",
                { className: "continue-button", type: "submit" },
                __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_continue_to_login" })
              )
            ),
            __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(
              "button",
              { className: "skip-button", disabled: !!this.state.emailInput, onClick: this.clickSkip },
              __WEBPACK_IMPORTED_MODULE_3_react___default.a.createElement(__WEBPACK_IMPORTED_MODULE_0_react_intl__["FormattedMessage"], { id: "firstrun_skip_login" })
            )
          )
        )
      )
    );
  }
}
/* unused harmony export _StartupOverlay */


const StartupOverlay = Object(__WEBPACK_IMPORTED_MODULE_2_react_redux__["connect"])()(Object(__WEBPACK_IMPORTED_MODULE_0_react_intl__["injectIntl"])(_StartupOverlay));
/* harmony export (immutable) */ __webpack_exports__["a"] = StartupOverlay;


/***/ }),
/* 39 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__ = __webpack_require__(1);
/* harmony import */ var __WEBPACK_IMPORTED_MODULE_1_common_PerfService_jsm__ = __webpack_require__(17);



const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

class DetectUserSessionStart {
  constructor(store, options = {}) {
    this._store = store;
    // Overrides for testing
    this.document = options.document || global.document;
    this._perfService = options.perfService || __WEBPACK_IMPORTED_MODULE_1_common_PerfService_jsm__["a" /* perfService */];
    this._onVisibilityChange = this._onVisibilityChange.bind(this);
  }

  /**
   * sendEventOrAddListener - Notify immediately if the page is already visible,
   *                    or else set up a listener for when visibility changes.
   *                    This is needed for accurate session tracking for telemetry,
   *                    because tabs are pre-loaded.
   */
  sendEventOrAddListener() {
    if (this.document.visibilityState === VISIBLE) {
      // If the document is already visible, to the user, send a notification
      // immediately that a session has started.
      this._sendEvent();
    } else {
      // If the document is not visible, listen for when it does become visible.
      this.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  /**
   * _sendEvent - Sends a message to the main process to indicate the current
   *              tab is now visible to the user, includes the
   *              visibility_event_rcvd_ts time in ms from the UNIX epoch.
   */
  _sendEvent() {
    this._perfService.mark("visibility_event_rcvd_ts");

    try {
      let visibility_event_rcvd_ts = this._perfService.getMostRecentAbsMarkStartByName("visibility_event_rcvd_ts");

      this._store.dispatch(__WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["b" /* actionCreators */].AlsoToMain({
        type: __WEBPACK_IMPORTED_MODULE_0_common_Actions_jsm__["c" /* actionTypes */].SAVE_SESSION_PERF_DATA,
        data: { visibility_event_rcvd_ts }
      }));
    } catch (ex) {
      // If this failed, it's likely because the `privacy.resistFingerprinting`
      // pref is true.  We should at least not blow up.
    }
  }

  /**
   * _onVisibilityChange - If the visibility has changed to visible, sends a notification
   *                      and removes the event listener. This should only be called once per tab.
   */
  _onVisibilityChange() {
    if (this.document.visibilityState === VISIBLE) {
      this._sendEvent();
      this.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }
}
/* harmony export (immutable) */ __webpack_exports__["a"] = DetectUserSessionStart;

/* WEBPACK VAR INJECTION */}.call(__webpack_exports__, __webpack_require__(3)))

/***/ })
/******/ ]);
//# sourceMappingURL=activity-stream.bundle.js.map