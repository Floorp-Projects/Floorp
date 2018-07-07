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
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
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
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var content_src_lib_snippets__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(3);
/* harmony import */ var content_src_components_Base_Base__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(12);
/* harmony import */ var content_src_lib_detect_user_session_start__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(37);
/* harmony import */ var content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(10);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_7___default = /*#__PURE__*/__webpack_require__.n(react_dom__WEBPACK_IMPORTED_MODULE_7__);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(41);










const store = Object(content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_4__["initStore"])(common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_8__["reducers"], global.gActivityStreamPrerenderedState);

new content_src_lib_detect_user_session_start__WEBPACK_IMPORTED_MODULE_3__["DetectUserSessionStart"](store).sendEventOrAddListener();

// If we are starting in a prerendered state, we must wait until the first render
// to request state rehydration (see Base.jsx). If we are NOT in a prerendered state,
// we can request it immedately.
if (!global.gActivityStreamPrerenderedState) {
  store.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST }));
}

react_dom__WEBPACK_IMPORTED_MODULE_7___default.a.hydrate(react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
  react_redux__WEBPACK_IMPORTED_MODULE_5__["Provider"],
  { store: store },
  react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_Base_Base__WEBPACK_IMPORTED_MODULE_2__["Base"], {
    isFirstrun: global.document.location.href === "about:welcome",
    isPrerendered: !!global.gActivityStreamPrerenderedState,
    locale: global.document.documentElement.lang,
    strings: global.gActivityStreamStrings })
), document.getElementById("root"));

Object(content_src_lib_snippets__WEBPACK_IMPORTED_MODULE_1__["addSnippetsSubscriber"])(store);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 1 */
/***/ (function(module, exports) {

var g;

// This works in non-strict mode
g = (function() {
	return this;
})();

try {
	// This works if eval is allowed (see CSP)
	g = g || Function("return this")() || (1, eval)("this");
} catch (e) {
	// This works if the window reference is available
	if (typeof window === "object") g = window;
}

// g can still be undefined, but nothing to do about it...
// We return undefined, instead of nothing here, so it's
// easier to handle this case. if(!global) { ...}

module.exports = g;


/***/ }),
/* 2 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MAIN_MESSAGE_TYPE", function() { return MAIN_MESSAGE_TYPE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CONTENT_MESSAGE_TYPE", function() { return CONTENT_MESSAGE_TYPE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PRELOAD_MESSAGE_TYPE", function() { return PRELOAD_MESSAGE_TYPE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "UI_CODE", function() { return UI_CODE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "BACKGROUND_PROCESS", function() { return BACKGROUND_PROCESS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "globalImportContext", function() { return globalImportContext; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "actionTypes", function() { return actionTypes; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterActions", function() { return ASRouterActions; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "actionCreators", function() { return actionCreators; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "actionUtils", function() { return actionUtils; });
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
// Export for tests

// Create an object that avoids accidental differing key/value pairs:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
const actionTypes = {};

for (const type of ["ADDONS_INFO_REQUEST", "ADDONS_INFO_RESPONSE", "ARCHIVE_FROM_POCKET", "AS_ROUTER_TELEMETRY_USER_EVENT", "BLOCK_URL", "BOOKMARK_URL", "COPY_DOWNLOAD_LINK", "DELETE_BOOKMARK_BY_ID", "DELETE_FROM_POCKET", "DELETE_HISTORY_URL", "DIALOG_CANCEL", "DIALOG_OPEN", "DISABLE_ONBOARDING", "DOWNLOAD_CHANGED", "INIT", "MIGRATION_CANCEL", "MIGRATION_COMPLETED", "MIGRATION_START", "NEW_TAB_INIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_REHYDRATED", "NEW_TAB_STATE_REQUEST", "NEW_TAB_UNLOAD", "OPEN_DOWNLOAD_FILE", "OPEN_LINK", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "OPEN_WEBEXT_SETTINGS", "PAGE_PRERENDERED", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINKS_CHANGED", "PLACES_LINK_BLOCKED", "PLACES_LINK_DELETED", "PLACES_SAVED_TO_POCKET", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "PREVIEW_REQUEST", "PREVIEW_REQUEST_CANCEL", "PREVIEW_RESPONSE", "REMOVE_DOWNLOAD_FILE", "RICH_ICON_MISSING", "SAVE_SESSION_PERF_DATA", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SECTION_DEREGISTER", "SECTION_DISABLE", "SECTION_ENABLE", "SECTION_MOVE", "SECTION_OPTIONS_CHANGED", "SECTION_REGISTER", "SECTION_UPDATE", "SECTION_UPDATE_CARD", "SETTINGS_CLOSE", "SETTINGS_OPEN", "SET_PREF", "SHOW_DOWNLOAD_FILE", "SHOW_FIREFOX_ACCOUNTS", "SKIPPED_SIGNIN", "SNIPPETS_BLOCKLIST_CLEARED", "SNIPPETS_BLOCKLIST_UPDATED", "SNIPPETS_DATA", "SNIPPETS_RESET", "SNIPPET_BLOCKED", "SUBMIT_EMAIL", "SYSTEM_TICK", "TELEMETRY_IMPRESSION_STATS", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "TOP_SITES_CANCEL_EDIT", "TOP_SITES_EDIT", "TOP_SITES_INSERT", "TOP_SITES_PIN", "TOP_SITES_PREFS_UPDATED", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "TOTAL_BOOKMARKS_REQUEST", "TOTAL_BOOKMARKS_RESPONSE", "UNINIT", "UPDATE_SECTION_PREFS", "WEBEXT_CLICK", "WEBEXT_DISMISS"]) {
  actionTypes[type] = type;
}

// These are acceptable actions for AS Router messages to have. They can show up
// as call-to-action buttons in snippets, onboarding tour, etc.
const ASRouterActions = {};

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
/* 3 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SNIPPETS_UPDATE_INTERVAL_MS", function() { return SNIPPETS_UPDATE_INTERVAL_MS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SnippetsMap", function() { return SnippetsMap; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SnippetsProvider", function() { return SnippetsProvider; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "addSnippetsSubscriber", function() { return addSnippetsSubscriber; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var content_src_asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

const DATABASE_NAME = "snippets_db";
const DATABASE_VERSION = 1;
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
const SNIPPETS_UPDATE_INTERVAL_MS = 14400000; // 4 hours.

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
    this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SNIPPETS_BLOCKLIST_CLEARED }));
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
  blockSnippetById(id) {
    var _this = this;

    return _asyncToGenerator(function* () {
      if (!id) {
        return;
      }
      const { blockList } = _this;
      if (!blockList.includes(id)) {
        blockList.push(id);
        _this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SNIPPETS_BLOCKLIST_UPDATED, data: id }));
        yield _this.set("blockList", blockList);
      }
    })();
  }

  disableOnboarding() {
    this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISABLE_ONBOARDING }));
  }

  showFirefoxAccounts() {
    this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SHOW_FIREFOX_ACCOUNTS }));
  }

  getTotalBookmarksCount() {
    return new Promise(resolve => {
      this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOTAL_BOOKMARKS_REQUEST }));
      global.RPMAddMessageListener("ActivityStream:MainToContent", function onMessage({ data: action }) {
        if (action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOTAL_BOOKMARKS_RESPONSE) {
          resolve(action.data);
          global.RPMRemoveMessageListener("ActivityStream:MainToContent", onMessage);
        }
      });
    });
  }

  getAddonsInfo() {
    return new Promise(resolve => {
      this._dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].ADDONS_INFO_REQUEST }));
      global.RPMAddMessageListener("ActivityStream:MainToContent", function onMessage({ data: action }) {
        if (action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].ADDONS_INFO_RESPONSE) {
          resolve(action.data);
          global.RPMRemoveMessageListener("ActivityStream:MainToContent", onMessage);
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
  connect() {
    var _this2 = this;

    return _asyncToGenerator(function* () {
      // Open the connection
      const db = yield _this2._openDB();

      // Restore any existing values
      yield _this2._restoreFromDb(db);

      // Attach a reference to the db
      _this2._db = db;
    })();
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

  _refreshSnippets() {
    var _this3 = this;

    return _asyncToGenerator(function* () {
      // Check if the cached version of of the snippets in snippetsMap. If it's too
      // old, blow away the entire snippetsMap.
      const cachedVersion = _this3.snippetsMap.get("snippets-cached-version");

      if (cachedVersion !== _this3.appData.version) {
        _this3.snippetsMap.clear();
      }

      // Has enough time passed for us to require an update?
      const lastUpdate = _this3.snippetsMap.get("snippets-last-update");
      const needsUpdate = !(lastUpdate >= 0) || Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS;

      if (needsUpdate && _this3.appData.snippetsURL) {
        _this3.snippetsMap.set("snippets-last-update", Date.now());
        try {
          const response = yield fetch(_this3.appData.snippetsURL);
          if (response.status === 200) {
            const payload = yield response.text();

            _this3.snippetsMap.set("snippets", payload);
            _this3.snippetsMap.set("snippets-cached-version", _this3.appData.version);
          }
        } catch (e) {
          console.error(e); // eslint-disable-line no-console
        }
      }
    })();
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
    if (msg.data.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SNIPPET_BLOCKED) {
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
  init(options) {
    var _this4 = this;

    return _asyncToGenerator(function* () {
      Object.assign(_this4, {
        appData: {},
        elementId: "snippets",
        connect: true
      }, options);

      // Add listener so we know when snippets are blocked on other pages
      if (global.RPMAddMessageListener) {
        global.RPMAddMessageListener("ActivityStream:MainToContent", _this4._onAction);
      }

      // TODO: Requires enabling indexedDB on newtab
      // Restore the snippets map from indexedDB
      if (_this4.connect) {
        try {
          yield _this4.snippetsMap.connect();
        } catch (e) {
          console.error(e); // eslint-disable-line no-console
        }
      }

      // Cache app data values so they can be accessible from gSnippetsMap
      for (const key of Object.keys(_this4.appData)) {
        if (key === "blockList") {
          _this4.snippetsMap.set("blockList", _this4.appData[key]);
        } else {
          _this4.snippetsMap.set(`appData.${key}`, _this4.appData[key]);
        }
      }

      // Refresh snippets, if enough time has passed.
      yield _this4._refreshSnippets();

      // Try showing remote snippets, falling back to defaults if necessary.
      try {
        _this4._showRemoteSnippets();
      } catch (e) {
        _this4._noSnippetFallback(e);
      }

      window.dispatchEvent(new Event(SNIPPETS_ENABLED_EVENT));

      _this4._forceOnboardingVisibility(true);
      _this4.initialized = true;
    })();
  }

  uninit() {
    window.dispatchEvent(new Event(SNIPPETS_DISABLED_EVENT));
    this._forceOnboardingVisibility(false);
    if (global.RPMRemoveMessageListener) {
      global.RPMRemoveMessageListener("ActivityStream:MainToContent", this._onAction);
    }
    this.initialized = false;
  }
}

/**
 * addSnippetsSubscriber - Creates a SnippetsProvider that Initializes
 *                         when the store has received the appropriate
 *                         Snippet data.
 *
 * @param  {obj} store   The redux store
 * @return {obj}         Returns the snippets instance, asrouterContent instance and unsubscribe function
 */
function addSnippetsSubscriber(store) {
  const snippets = new SnippetsProvider(store.dispatch);
  const asrouterContent = new content_src_asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_1__["ASRouterContent"]();

  let initializing = false;

  store.subscribe(_asyncToGenerator(function* () {
    const state = store.getState();
    const isASRouterEnabled = state.Prefs.values.asrouterExperimentEnabled && state.Prefs.values.asrouterOnboardingCohort > 0;
    // state.Prefs.values["feeds.snippets"]:  Should snippets be shown?
    // state.Snippets.initialized             Is the snippets data initialized?
    // snippets.initialized:                  Is SnippetsProvider currently initialised?
    if (state.Prefs.values["feeds.snippets"] &&
    // If the message center experiment is enabled, don't show snippets
    !isASRouterEnabled && !state.Prefs.values.disableSnippets && state.Snippets.initialized && !snippets.initialized &&
    // Don't call init multiple times
    !initializing && location.href !== "about:welcome") {
      initializing = true;
      yield snippets.init({ appData: state.Snippets });
      initializing = false;
    } else if ((state.Prefs.values["feeds.snippets"] === false || state.Prefs.values.disableSnippets === true) && snippets.initialized) {
      snippets.uninit();
    }

    // Turn on AS Router snippets if the experiment is enabled and the snippets pref is on;
    // otherwise, turn it off.
    if ((state.Prefs.values.asrouterExperimentEnabled || state.Prefs.values.asrouterOnboardingCohort > 0) && state.Prefs.values["feeds.snippets"] && !asrouterContent.initialized) {
      asrouterContent.init();
    } else if ((!state.Prefs.values.asrouterExperimentEnabled && state.Prefs.values.asrouterOnboardingCohort === 0 || !state.Prefs.values["feeds.snippets"]) && asrouterContent.initialized) {
      asrouterContent.uninit();
    }
  }));

  // These values are returned for testing purposes
  return { snippets, asrouterContent };
}
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 4 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterUtils", function() { return ASRouterUtils; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "convertLinks", function() { return convertLinks; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterUISurface", function() { return ASRouterUISurface; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterContent", function() { return ASRouterContent; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var fluent_react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(39);
/* harmony import */ var content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(7);
/* harmony import */ var _components_ImpressionsWrapper_ImpressionsWrapper__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(9);
/* harmony import */ var fluent__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(38);
/* harmony import */ var _templates_OnboardingMessage_OnboardingMessage__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(42);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(10);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_7___default = /*#__PURE__*/__webpack_require__.n(react_dom__WEBPACK_IMPORTED_MODULE_7__);
/* harmony import */ var _template_utils__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(11);
/* harmony import */ var _templates_SimpleSnippet_SimpleSnippet__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(40);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };












const INCOMING_MESSAGE_NAME = "ASRouter:parent-to-child";
const OUTGOING_MESSAGE_NAME = "ASRouter:child-to-parent";

const ASRouterUtils = {
  addListener(listener) {
    global.RPMAddMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  removeListener(listener) {
    global.RPMRemoveMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  sendMessage(action) {
    global.RPMSendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  },
  blockById(id) {
    ASRouterUtils.sendMessage({ type: "BLOCK_MESSAGE_BY_ID", data: { id } });
  },
  blockBundle(bundle) {
    ASRouterUtils.sendMessage({ type: "BLOCK_BUNDLE", data: { bundle } });
  },
  executeAction({ button_action, button_action_params }) {
    if (button_action in common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["ASRouterActions"]) {
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
    const payload = common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ASRouterUserEvent(ping);
    global.RPMSendAsyncMessage(content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_2__["OUTGOING_MESSAGE_NAME"], payload);
  },
  getEndpoint() {
    if (window.location.href.includes("endpoint")) {
      const params = new URLSearchParams(window.location.href.slice(window.location.href.indexOf("endpoint")));
      try {
        const endpoint = new URL(params.get("endpoint"));
        return {
          url: endpoint.href,
          snippetId: params.get("snippetId")
        };
      } catch (e) {}
    }

    return null;
  }
};

// Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />
function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id);
}

function generateMessages(content) {
  const cx = new fluent__WEBPACK_IMPORTED_MODULE_4__["MessageContext"]("en-US");
  cx.addMessages(`RichTextSnippet = ${content}`);
  return [cx];
}

// Elements allowed in snippet content
const ALLOWED_TAGS = {
  b: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("b", null),
  i: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("i", null),
  u: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("u", null),
  strong: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("strong", null),
  em: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("em", null),
  br: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("br", null)
};

/**
 * Transform an object (tag name: {url}) into (tag name: anchor) where the url
 * is used as href, in order to render links inside a Fluent.Localized component.
 */
function convertLinks(links, sendClick) {
  if (links) {
    return Object.keys(links).reduce((acc, linkTag) => {
      acc[linkTag] = react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("a", { href: Object(_template_utils__WEBPACK_IMPORTED_MODULE_8__["safeURI"])(links[linkTag].url), "data-metric": links[linkTag].metric, onClick: sendClick });
      return acc;
    }, {});
  }

  return null;
}

/**
 * Message wrapper used to sanitize markup and render HTML.
 */
function RichText(props) {
  return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
    fluent_react__WEBPACK_IMPORTED_MODULE_1__["Localized"],
    _extends({ id: "RichTextSnippet" }, ALLOWED_TAGS, convertLinks(props.links, props.sendClick)),
    react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
      "span",
      null,
      props.text
    )
  );
}

class ASRouterUISurface extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.sendClick = this.sendClick.bind(this);
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
      source: extraProps.id,
      action: eventType
    }, extraProps));
  }

  sendImpression(extraProps) {
    this.sendUserActionTelemetry(Object.assign({ event: "IMPRESSION" }, extraProps));
  }

  // If link has a `metric` data attribute send it as part of the `value`
  // telemetry field which can have arbitrary values.
  // Used for router messages with links as part of the content.
  sendClick(event) {
    const metric = {
      value: event.target.dataset.metric,
      // Used for the `source` of the event. Needed to differentiate
      // from other snippet or onboarding events that may occur.
      id: "NEWTAB_FOOTER_BAR_CONTENT"
    };
    this.sendUserActionTelemetry(Object.assign({ event: "CLICK_BUTTON" }, metric));
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
        if (action.data.id === this.state.message.id) {
          this.setState({ message: {} });
        }
        break;
      case "CLEAR_BUNDLE":
        if (this.state.bundle.bundle) {
          this.setState({ bundle: {} });
        }
        break;
      case "CLEAR_ALL":
        this.setState({ message: {}, bundle: {} });
    }
  }

  componentWillMount() {
    const endpoint = ASRouterUtils.getEndpoint();
    ASRouterUtils.addListener(this.onMessageFromParent);

    // If we are loading about:welcome we want to trigger the onboarding messages
    if (this.props.document.location.href === "about:welcome") {
      ASRouterUtils.sendMessage({ type: "TRIGGER", data: { trigger: "firstRun" } });
    } else {
      ASRouterUtils.sendMessage({ type: "CONNECT_UI_REQUEST", data: { endpoint } });
    }
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  renderSnippets() {
    return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
      _components_ImpressionsWrapper_ImpressionsWrapper__WEBPACK_IMPORTED_MODULE_3__["ImpressionsWrapper"],
      {
        id: "NEWTAB_FOOTER_BAR",
        message: this.state.message,
        sendImpression: this.sendImpression,
        shouldSendImpressionOnUpdate: shouldSendImpressionOnUpdate
        // This helps with testing
        , document: this.props.document },
      react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
        fluent_react__WEBPACK_IMPORTED_MODULE_1__["LocalizationProvider"],
        { messages: generateMessages(this.state.message.content.text) },
        react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_templates_SimpleSnippet_SimpleSnippet__WEBPACK_IMPORTED_MODULE_9__["SimpleSnippet"], _extends({}, this.state.message, {
          richText: react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(RichText, { text: this.state.message.content.text,
            links: this.state.message.content.links,
            sendClick: this.sendClick }),
          UISurface: "NEWTAB_FOOTER_BAR",
          getNextMessage: ASRouterUtils.getNextMessage,
          onBlock: this.onBlockById(this.state.message.id),
          sendUserActionTelemetry: this.sendUserActionTelemetry }))
      )
    );
  }

  renderOnboarding() {
    return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_templates_OnboardingMessage_OnboardingMessage__WEBPACK_IMPORTED_MODULE_5__["OnboardingMessage"], _extends({}, this.state.bundle, {
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

ASRouterUISurface.defaultProps = { document: global.document };

class ASRouterContent {
  constructor() {
    this.initialized = false;
    this.containerElement = null;
  }

  _mount() {
    this.containerElement = global.document.getElementById("snippets-container");
    react_dom__WEBPACK_IMPORTED_MODULE_7___default.a.render(react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(ASRouterUISurface, null), this.containerElement);
  }

  _unmount() {
    react_dom__WEBPACK_IMPORTED_MODULE_7___default.a.unmountComponentAtNode(this.containerElement);
  }

  init() {
    this._mount();
    this.initialized = true;
  }

  uninit() {
    if (this.initialized) {
      this._unmount();
      this.initialized = false;
    }
  }
}
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 5 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 6 */
/***/ (function(module, exports) {

module.exports = PropTypes;

/***/ }),
/* 7 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MERGE_STORE_ACTION", function() { return MERGE_STORE_ACTION; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "OUTGOING_MESSAGE_NAME", function() { return OUTGOING_MESSAGE_NAME; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "INCOMING_MESSAGE_NAME", function() { return INCOMING_MESSAGE_NAME; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "EARLY_QUEUED_ACTIONS", function() { return EARLY_QUEUED_ACTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "rehydrationMiddleware", function() { return rehydrationMiddleware; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "queueEarlyMessageMiddleware", function() { return queueEarlyMessageMiddleware; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "initStore", function() { return initStore; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(redux__WEBPACK_IMPORTED_MODULE_1__);
/* eslint-env mozilla/frame-script */




const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
const EARLY_QUEUED_ACTIONS = [common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA, common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].PAGE_PRERENDERED];

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
  if (common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isSendToMain(action)) {
    RPMSendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
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
  const isRehydrationRequest = action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST;

  if (isRehydrationRequest) {
    store._didRequestInitialState = true;
    return next(action);
  }

  if (isMergeStoreAction) {
    store._didRehydrate = true;
    return next(action);
  }

  // If init happened after our request was made, we need to re-request
  if (store._didRequestInitialState && action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].INIT) {
    return next(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST }));
  }

  if (common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isBroadcastToContent(action) || common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isSendToOneContent(action) || common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isSendToPreloaded(action)) {
    // Note that actions received before didRehydrate will not be dispatched
    // because this could negatively affect preloading and the the state
    // will be replaced by rehydration anyway.
    return null;
  }

  return next(action);
};

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
  } else if (common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isFromMain(action)) {
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

/**
 * initStore - Create a store and listen for incoming actions
 *
 * @param  {object} reducers An object containing Redux reducers
 * @param  {object} intialState (optional) The initial state of the store, if desired
 * @return {object}          A redux store
 */
function initStore(reducers, initialState) {
  const store = Object(redux__WEBPACK_IMPORTED_MODULE_1__["createStore"])(mergeStateReducer(Object(redux__WEBPACK_IMPORTED_MODULE_1__["combineReducers"])(reducers)), initialState, global.RPMAddMessageListener && Object(redux__WEBPACK_IMPORTED_MODULE_1__["applyMiddleware"])(rehydrationMiddleware, queueEarlyMessageMiddleware, messageMiddleware));

  store._didRehydrate = false;
  store._didRequestInitialState = false;

  if (global.RPMAddMessageListener) {
    global.RPMAddMessageListener(INCOMING_MESSAGE_NAME, msg => {
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
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 8 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 9 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "VISIBLE", function() { return VISIBLE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "VISIBILITY_CHANGE_EVENT", function() { return VISIBILITY_CHANGE_EVENT; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ImpressionsWrapper", function() { return ImpressionsWrapper; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

/**
 * Component wrapper used to send telemetry pings on every impression.
 */
class ImpressionsWrapper extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  // This sends an event when a user sees a set of new content. If content
  // changes while the page is hidden (i.e. preloaded or on a hidden tab),
  // only send the event if the page becomes visible again.
  sendImpressionOrAddListener() {
    if (this.props.document.visibilityState === VISIBLE) {
      this.props.sendImpression({ id: this.props.id });
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      // When the page becomes visible, send the impression stats ping if the section isn't collapsed.
      this._onVisibilityChange = () => {
        if (this.props.document.visibilityState === VISIBLE) {
          this.props.sendImpression({ id: this.props.id });
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

ImpressionsWrapper.defaultProps = {
  document: global.document,
  sendOnMount: true
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 10 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 11 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "safeURI", function() { return safeURI; });
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

/***/ }),
/* 12 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Base", function() { return _Base; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "BaseContent", function() { return BaseContent; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Base", function() { return Base; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var content_src_components_ASRouterAdmin_ASRouterAdmin__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(14);
/* harmony import */ var content_src_components_ConfirmDialog_ConfirmDialog__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(15);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(17);
/* harmony import */ var content_src_components_ManualMigration_ManualMigration__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(18);
/* harmony import */ var common_PrerenderData_jsm__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(19);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_8__);
/* harmony import */ var content_src_components_Search_Search__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(20);
/* harmony import */ var content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(22);
/* harmony import */ var content_src_components_StartupOverlay_StartupOverlay__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(36);













const PrefsButton = Object(react_intl__WEBPACK_IMPORTED_MODULE_1__["injectIntl"])(props => react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
  "div",
  { className: "prefs-button" },
  react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("button", { className: "icon icon-settings", onClick: props.onClick, title: props.intl.formatMessage({ id: "settings_pane_button_label" }) })
));

// Add the locale data for pluralization and relative-time formatting for now,
// this just uses english locale data. We can make this more sophisticated if
// more features are needed.
function addLocaleDataForReactIntl(locale) {
  Object(react_intl__WEBPACK_IMPORTED_MODULE_1__["addLocaleData"])([{ locale, parentLocale: "en" }]);
}

class _Base extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  componentWillMount() {
    const { App, locale } = this.props;
    this.sendNewTabRehydrated(App);
    addLocaleDataForReactIntl(locale);
    if (this.props.isFirstrun) {
      global.document.body.classList.add("welcome");
    }
  }

  componentDidMount() {
    // Request state AFTER the first render to ensure we don't cause the
    // prerendered DOM to be unmounted. Otherwise, NEW_TAB_STATE_REQUEST is
    // dispatched right after the store is ready.
    if (this.props.isPrerendered) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].PAGE_PRERENDERED }));
    }
  }

  componentWillUnmount() {
    this.updateTheme();
  }

  componentWillUpdate({ App }) {
    this.updateTheme();
    this.sendNewTabRehydrated(App);
  }

  updateTheme() {
    const bodyClassName = ["activity-stream",
    // If we skipped the about:welcome overlay and removed the CSS class
    // we don't want to add it back to the Activity Stream view
    document.body.classList.contains("welcome") ? "welcome" : ""].filter(v => v).join(" ");
    global.document.body.className = bodyClassName;
  }

  // The NEW_TAB_REHYDRATED event is used to inform feeds that their
  // data has been consumed e.g. for counting the number of tabs that
  // have rendered that data.
  sendNewTabRehydrated(App) {
    if (App && App.initialized && !this.renderNotified) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_REHYDRATED, data: {} }));
      this.renderNotified = true;
    }
  }

  render() {
    const { props } = this;
    const { App, locale, strings } = props;
    const { initialized } = App;

    const prefs = props.Prefs.values;
    if ((prefs.asrouterExperimentEnabled || prefs.asrouterOnboardingCohort > 0) && window.location.hash === "#asrouter") {
      return react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ASRouterAdmin_ASRouterAdmin__WEBPACK_IMPORTED_MODULE_2__["ASRouterAdmin"], null);
    }

    if (!props.isPrerendered && !initialized) {
      return null;
    }

    // Until we can delete the existing onboarding tour, just hide the onboarding button when users are in
    // the new simplified onboarding experiment. CSS hacks ftw
    if (prefs.asrouterOnboardingCohort > 0) {
      global.document.body.classList.add("hide-onboarding");
    }

    return react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
      react_intl__WEBPACK_IMPORTED_MODULE_1__["IntlProvider"],
      { locale: locale, messages: strings },
      react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
        content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_5__["ErrorBoundary"],
        { className: "base-content-fallback" },
        react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(BaseContent, this.props)
      )
    );
  }
}

class BaseContent extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
  }

  openPreferences() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SETTINGS_OPEN }));
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({ event: "OPEN_NEWTAB_PREFS" }));
  }

  render() {
    const { props } = this;
    const { App } = props;
    const { initialized } = App;
    const prefs = props.Prefs.values;

    const shouldBeFixedToTop = common_PrerenderData_jsm__WEBPACK_IMPORTED_MODULE_7__["PrerenderData"].arePrefsValid(name => prefs[name]);

    const outerClassName = ["outer-wrapper", shouldBeFixedToTop && "fixed-to-top"].filter(v => v).join(" ");

    return react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
      "div",
      null,
      react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
        "div",
        { className: outerClassName },
        react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
          "main",
          null,
          prefs.showSearch && react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
            "div",
            { className: "non-collapsible-section" },
            react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
              content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_5__["ErrorBoundary"],
              null,
              react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Search_Search__WEBPACK_IMPORTED_MODULE_9__["Search"], null)
            )
          ),
          react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
            "div",
            { className: `body-wrapper${initialized ? " on" : ""}` },
            !prefs.migrationExpired && react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(
              "div",
              { className: "non-collapsible-section" },
              react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ManualMigration_ManualMigration__WEBPACK_IMPORTED_MODULE_6__["ManualMigration"], null)
            ),
            react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_10__["Sections"], null),
            react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(PrefsButton, { onClick: this.openPreferences })
          ),
          react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ConfirmDialog_ConfirmDialog__WEBPACK_IMPORTED_MODULE_3__["ConfirmDialog"], null)
        )
      ),
      this.props.isFirstrun && react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_StartupOverlay_StartupOverlay__WEBPACK_IMPORTED_MODULE_11__["StartupOverlay"], null)
    );
  }
}

const Base = Object(react_redux__WEBPACK_IMPORTED_MODULE_4__["connect"])(state => ({ App: state.App, Prefs: state.Prefs }))(_Base);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 13 */
/***/ (function(module, exports) {

module.exports = ReactIntl;

/***/ }),
/* 14 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterAdmin", function() { return ASRouterAdmin; });
/* harmony import */ var _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(4);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);



class ASRouterAdmin extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
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
    const endpoint = _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].getEndpoint();
    _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].sendMessage({ type: "ADMIN_CONNECT_STATE", data: { endpoint } });
    _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].addListener(this.onMessage);
  }

  componentWillUnmount() {
    _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].removeListener(this.onMessage);
  }

  findOtherBundledMessagesOfSameTemplate(template) {
    return this.state.messages.filter(msg => msg.template === template && msg.bundled);
  }

  handleBlock(msg) {
    if (msg.bundled) {
      // If we are blocking a message that belongs to a bundle, block all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].blockBundle(bundle);
    }
    return () => _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].blockById(msg.id);
  }

  handleUnblock(msg) {
    if (msg.bundled) {
      // If we are unblocking a message that belongs to a bundle, unblock all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].unblockBundle(bundle);
    }
    return () => _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].unblockById(msg.id);
  }

  handleOverride(id) {
    return () => _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].overrideMessage(id);
  }

  renderMessageItem(msg) {
    const isCurrent = msg.id === this.state.lastMessageId;
    const isBlocked = this.state.blockList.includes(msg.id);

    let itemClassName = "message-item";
    if (isCurrent) {
      itemClassName += " current";
    }
    if (isBlocked) {
      itemClassName += " blocked";
    }

    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "tr",
      { className: itemClassName, key: msg.id },
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "td",
        { className: "message-id" },
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
          "span",
          null,
          msg.id
        )
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "td",
        null,
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
          "button",
          { className: `button ${isBlocked ? "" : " primary"}`, onClick: isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg) },
          isBlocked ? "Unblock" : "Block"
        ),
        isBlocked ? null : react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
          "button",
          { className: "button", onClick: this.handleOverride(msg.id) },
          "Show"
        )
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "td",
        { className: "message-summary" },
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
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
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "table",
      null,
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "tbody",
        null,
        this.state.messages.map(msg => this.renderMessageItem(msg))
      )
    );
  }

  renderProviders() {
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "table",
      null,
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "tbody",
        null,
        this.state.providers.map((provider, i) => react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
          "tr",
          { className: "message-item", key: i },
          react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
            "td",
            null,
            provider.id
          ),
          react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
            "td",
            null,
            provider.type === "remote" ? react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
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
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "div",
      { className: "asrouter-admin outer-wrapper" },
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "h1",
        null,
        "AS Router Admin"
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "button",
        { className: "button primary", onClick: _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_0__["ASRouterUtils"].getNextMessage },
        "Refresh Current Message"
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "h2",
        null,
        "Message Providers"
      ),
      this.state.providers ? this.renderProviders() : null,
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "h2",
        null,
        "Messages"
      ),
      this.renderMessages()
    );
  }
}

/***/ }),
/* 15 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_ConfirmDialog", function() { return _ConfirmDialog; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ConfirmDialog", function() { return ConfirmDialog; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);





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
class _ConfirmDialog extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_CANCEL });
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({ event: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_CANCEL, source: this.props.data.eventSource }));
  }

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  }

  _renderModalMessage() {
    const message_body = this.props.data.body_string_id;

    if (!message_body) {
      return null;
    }

    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "span",
      null,
      message_body.map(msg => react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "p",
        { key: msg },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: msg })
      ))
    );
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "div",
      { className: "confirmation-dialog" },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", { className: "modal-overlay", onClick: this._handleCancelBtn }),
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "div",
        { className: "modal" },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "section",
          { className: "modal-message" },
          this.props.data.icon && react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", { className: `icon icon-spacer icon-${this.props.data.icon}` }),
          this._renderModalMessage()
        ),
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "section",
          { className: "actions" },
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "button",
            { onClick: this._handleCancelBtn },
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: this.props.data.cancel_button_string_id })
          ),
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "button",
            { className: "done", onClick: this._handleConfirmBtn },
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: this.props.data.confirm_button_string_id })
          )
        )
      )
    );
  }
}

const ConfirmDialog = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(state => state.Dialog)(_ConfirmDialog);

/***/ }),
/* 16 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 17 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ErrorBoundaryFallback", function() { return ErrorBoundaryFallback; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ErrorBoundary", function() { return ErrorBoundary; });
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);



class ErrorBoundaryFallback extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
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
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "div",
      { className: className },
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "div",
        null,
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], {
          defaultMessage: "Oops, something went wrong loading this content.",
          id: "error_fallback_default_info" })
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "span",
        null,
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
          "a",
          { href: "#", className: "reload-button", onClick: this.onClick },
          react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], {
            defaultMessage: "Refresh page to try again.",
            id: "error_fallback_default_refresh_suggestion" })
        )
      )
    );
  }
}
ErrorBoundaryFallback.defaultProps = { className: "as-error-fallback" };

class ErrorBoundary extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
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

    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(this.props.FallbackComponent, { className: this.props.className });
  }
}

ErrorBoundary.defaultProps = { FallbackComponent: ErrorBoundaryFallback };

/***/ }),
/* 18 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_ManualMigration", function() { return _ManualMigration; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ManualMigration", function() { return ManualMigration; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);





/**
 * Manual migration component used to start the profile import wizard.
 * Message is presented temporarily and will go away if:
 * 1.  User clicks "No Thanks"
 * 2.  User completed the data import
 * 3.  After 3 active days
 * 4.  User clicks "Cancel" on the import wizard (currently not implemented).
 */
class _ManualMigration extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLaunchTour = this.onLaunchTour.bind(this);
    this.onCancelTour = this.onCancelTour.bind(this);
  }

  onLaunchTour() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].MIGRATION_START }));
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({ event: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].MIGRATION_START }));
  }

  onCancelTour() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].MIGRATION_CANCEL }));
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({ event: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].MIGRATION_CANCEL }));
  }

  render() {
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "div",
      { className: "manual-migration-container" },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "p",
        null,
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", { className: "icon icon-import" }),
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: "manual_migration_explanation2" })
      ),
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "div",
        { className: "manual-migration-actions actions" },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "button",
          { className: "dismiss", onClick: this.onCancelTour },
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: "manual_migration_cancel_button" })
        ),
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "button",
          { onClick: this.onLaunchTour },
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_2__["FormattedMessage"], { id: "manual_migration_import_button" })
        )
      )
    );
  }
}

const ManualMigration = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])()(_ManualMigration);

/***/ }),
/* 19 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_PrerenderData", function() { return _PrerenderData; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PrerenderData", function() { return PrerenderData; });
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
/* 20 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Search", function() { return _Search; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Search", function() { return Search; });
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var content_src_lib_constants__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(21);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* globals ContentSearchUIController */








class _Search extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
  }

  handleEvent(event) {
    // Also track search events with our own telemetry
    if (event.detail.type === "Search") {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({ event: "SEARCH" }));
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
      const healthReportKey = content_src_lib_constants__WEBPACK_IMPORTED_MODULE_3__["IS_NEWTAB"] ? "newtab" : "abouthome";

      // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)
      const searchSource = content_src_lib_constants__WEBPACK_IMPORTED_MODULE_3__["IS_NEWTAB"] ? "newtab" : "homepage";

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
    return react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
      "div",
      { className: "search-wrapper" },
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
        "label",
        { htmlFor: "newtab-search-text", className: "search-label" },
        react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
          "span",
          { className: "sr-only" },
          react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "search_web_placeholder" })
        )
      ),
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("input", {
        id: "newtab-search-text",
        maxLength: "256",
        placeholder: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        ref: this.onInputMount,
        title: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        type: "search" }),
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
        "button",
        {
          id: "searchSubmit",
          className: "search-button",
          onClick: this.onClick,
          title: this.props.intl.formatMessage({ id: "search_button" }) },
        react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
          "span",
          { className: "sr-only" },
          react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "search_button" })
        )
      )
    );
  }
}

const Search = Object(react_redux__WEBPACK_IMPORTED_MODULE_2__["connect"])()(Object(react_intl__WEBPACK_IMPORTED_MODULE_0__["injectIntl"])(_Search));

/***/ }),
/* 21 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "IS_NEWTAB", function() { return IS_NEWTAB; });
const IS_NEWTAB = global.document && global.document.documentURI === "about:newtab";
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 22 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Section", function() { return Section; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionIntl", function() { return SectionIntl; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Sections", function() { return _Sections; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Sections", function() { return Sections; });
/* harmony import */ var content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(43);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(2);
/* harmony import */ var content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(27);
/* harmony import */ var content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(30);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var content_src_components_Topics_Topics__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(32);
/* harmony import */ var content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(33);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };











const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const CARDS_PER_ROW_DEFAULT = 3;
const CARDS_PER_ROW_COMPACT_WIDE = 4;

function getFormattedMessage(message) {
  return typeof message === "string" ? react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
    "span",
    null,
    message
  ) : react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_1__["FormattedMessage"], message);
}

class Section extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
  get numRows() {
    const { rowsPref, maxRows, Prefs } = this.props;
    return rowsPref ? Prefs.values[rowsPref] : maxRows;
  }

  _dispatchImpressionStats() {
    const { props } = this;
    let cardsPerRow = CARDS_PER_ROW_DEFAULT;
    if (props.compactCards && global.matchMedia(`(min-width: 1072px)`).matches) {
      // If the section has compact cards and the viewport is wide enough, we show
      // 4 columns instead of 3.
      // $break-point-widest = 1072px (from _variables.scss)
      cardsPerRow = CARDS_PER_ROW_COMPACT_WIDE;
    }
    const maxCards = cardsPerRow * this.numRows;
    const cards = props.rows.slice(0, maxCards);

    if (this.needsImpressionStats(cards)) {
      props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_2__["actionCreators"].ImpressionStats({
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

  render() {
    const {
      id, eventSource, title, icon, rows,
      emptyState, dispatch, compactCards,
      contextMenuOptions, initialized, disclaimer,
      pref, privacyNoticeURL, isFirst, isLast
    } = this.props;

    const maxCardsPerRow = compactCards ? CARDS_PER_ROW_COMPACT_WIDE : CARDS_PER_ROW_DEFAULT;
    const { numRows } = this;
    const maxCards = maxCardsPerRow * numRows;
    const maxCardsOnNarrow = CARDS_PER_ROW_DEFAULT * numRows;

    // Show topics only for top stories and if it's not initialized yet (so
    // content doesn't shift when it is loaded) or has loaded with topics
    const shouldShowTopics = id === "topstories" && (!this.props.topics || this.props.topics.length > 0);

    const realRows = rows.slice(0, maxCards);

    // The empty state should only be shown after we have initialized and there is no content.
    // Otherwise, we should show placeholders.
    const shouldShowEmptyState = initialized && !rows.length;

    const cards = [];
    if (!shouldShowEmptyState) {
      for (let i = 0; i < maxCards; i++) {
        const link = realRows[i];
        // On narrow viewports, we only show 3 cards per row. We'll mark the rest as
        // .hide-for-narrow to hide in CSS via @media query.
        const className = i >= maxCardsOnNarrow ? "hide-for-narrow" : "";
        cards.push(link ? react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_0__["Card"], { key: i,
          index: i,
          className: className,
          dispatch: dispatch,
          link: link,
          contextMenuOptions: contextMenuOptions,
          eventSource: eventSource,
          shouldSendImpressionStats: this.props.shouldSendImpressionStats,
          isWebExtension: this.props.isWebExtension }) : react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_0__["PlaceholderCard"], { key: i, className: className }));
      }
    }

    const sectionClassName = ["section", compactCards ? "compact-cards" : "normal-cards"].join(" ");

    // <Section> <-- React component
    // <section> <-- HTML5 element
    return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
      content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_4__["ComponentPerfTimer"],
      this.props,
      react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
        content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_3__["CollapsibleSection"],
        { className: sectionClassName, icon: icon,
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
        !shouldShowEmptyState && react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
          "ul",
          { className: "section-list", style: { padding: 0 } },
          cards
        ),
        shouldShowEmptyState && react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
          "div",
          { className: "section-empty-state" },
          react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
            "div",
            { className: "empty-state" },
            emptyState.icon && emptyState.icon.startsWith("moz-extension://") ? react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("img", { className: "empty-state-icon icon", style: { "background-image": `url('${emptyState.icon}')` } }) : react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("img", { className: `empty-state-icon icon icon-${emptyState.icon}` }),
            react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
              "p",
              { className: "empty-state-message" },
              getFormattedMessage(emptyState.message)
            )
          )
        ),
        shouldShowTopics && react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_Topics_Topics__WEBPACK_IMPORTED_MODULE_7__["Topics"], { topics: this.props.topics, read_more_endpoint: this.props.read_more_endpoint })
      )
    );
  }
}

Section.defaultProps = {
  document: global.document,
  rows: [],
  emptyState: {},
  pref: {},
  title: ""
};

const SectionIntl = Object(react_redux__WEBPACK_IMPORTED_MODULE_5__["connect"])(state => ({ Prefs: state.Prefs }))(Object(react_intl__WEBPACK_IMPORTED_MODULE_1__["injectIntl"])(Section));

class _Sections extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
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
        sections.push(react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_8__["TopSites"], commonProps));
      } else {
        const section = enabledSections.find(s => s.id === sectionId);
        if (section) {
          sections.push(react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(SectionIntl, _extends({}, section, commonProps)));
        }
      }
    }
    return sections;
  }

  render() {
    return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
      "div",
      { className: "sections-list" },
      this.renderSections()
    );
  }
}

const Sections = Object(react_redux__WEBPACK_IMPORTED_MODULE_5__["connect"])(state => ({ Sections: state.Sections, Prefs: state.Prefs }))(_Sections);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 23 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "GetPlatformString", function() { return GetPlatformString; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "LinkMenuOptions", function() { return LinkMenuOptions; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);


const _OpenInPrivateWindow = site => ({
  id: "menu_action_open_private_window",
  icon: "new-window-private",
  action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
    type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_PRIVATE_WINDOW,
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
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "menu_action_bookmark",
    icon: "bookmark-hollow",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].BOOKMARK_URL,
      data: { url: site.url, title: site.title, type: site.type }
    }),
    userEvent: "BOOKMARK_ADD"
  }),
  OpenInNewWindow: site => ({
    id: "menu_action_open_new_window",
    icon: "new-window",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_NEW_WINDOW,
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
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].BLOCK_URL,
      data: { url: site.url, pocket_id: site.pocket_id }
    }),
    impression: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
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
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].WebExtEvent(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].WEBEXT_DISMISS, {
      source: eventSource,
      url: site.url,
      action_position: index
    })
  }),
  DeleteUrl: (site, index, eventSource, isEnabled, siteInfo) => ({
    id: "menu_action_delete",
    icon: "delete",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_OPEN,
      data: {
        onConfirm: [common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_HISTORY_URL, data: { url: site.url, pocket_id: site.pocket_id, forceBlock: site.bookmarkGuid } }), common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(Object.assign({ event: "DELETE", source: eventSource, action_position: index }, siteInfo))],
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
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SHOW_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  OpenFile: site => ({
    id: "menu_action_open_file",
    icon: "open-file",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  CopyDownloadLink: site => ({
    id: "menu_action_copy_download_link",
    icon: "copy",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].COPY_DOWNLOAD_LINK,
      data: { url: site.url }
    })
  }),
  GoToDownloadPage: site => ({
    id: "menu_action_go_to_download_page",
    icon: "download",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
      data: { url: site.referrer }
    }),
    disabled: !site.referrer
  }),
  RemoveDownload: site => ({
    id: "menu_action_remove_download",
    icon: "delete",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].REMOVE_DOWNLOAD_FILE,
      data: { url: site.url }
    })
  }),
  PinTopSite: (site, index) => ({
    id: "menu_action_pin",
    icon: "pin",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_PIN,
      data: { site: { url: site.url }, index }
    }),
    userEvent: "PIN"
  }),
  UnpinTopSite: site => ({
    id: "menu_action_unpin",
    icon: "unpin",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_UNPIN,
      data: { site: { url: site.url } }
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: (site, index, eventSource) => ({
    id: "menu_action_save_to_pocket",
    icon: "pocket",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_TO_POCKET,
      data: { site: { url: site.url, title: site.title } }
    }),
    impression: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
      source: eventSource,
      pocket: 0,
      tiles: [{ id: site.guid, pos: index }]
    }),
    userEvent: "SAVE_TO_POCKET"
  }),
  DeleteFromPocket: site => ({
    id: "menu_action_delete_pocket",
    icon: "delete",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_FROM_POCKET,
      data: { pocket_id: site.pocket_id }
    }),
    userEvent: "DELETE_FROM_POCKET"
  }),
  ArchiveFromPocket: site => ({
    id: "menu_action_archive_pocket",
    icon: "check",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].ARCHIVE_FROM_POCKET,
      data: { pocket_id: site.pocket_id }
    }),
    userEvent: "ARCHIVE_FROM_POCKET"
  }),
  EditTopSite: (site, index) => ({
    id: "edit_topsites_button_text",
    icon: "edit",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT,
      data: { index }
    }
  }),
  CheckBookmark: site => site.bookmarkGuid ? LinkMenuOptions.RemoveBookmark(site) : LinkMenuOptions.AddBookmark(site),
  CheckPinTopSite: (site, index) => site.isPinned ? LinkMenuOptions.UnpinTopSite(site) : LinkMenuOptions.PinTopSite(site, index),
  CheckSavedToPocket: (site, index) => site.pocket_id ? LinkMenuOptions.DeleteFromPocket(site) : LinkMenuOptions.SaveToPocket(site, index),
  CheckBookmarkOrArchive: site => site.pocket_id ? LinkMenuOptions.ArchiveFromPocket(site) : LinkMenuOptions.CheckBookmark(site),
  OpenInPrivateWindow: (site, index, eventSource, isEnabled) => isEnabled ? _OpenInPrivateWindow(site) : LinkMenuOptions.EmptyItem()
};

/***/ }),
/* 24 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_LinkMenu", function() { return _LinkMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "LinkMenu", function() { return LinkMenu; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(25);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(23);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_5__);







const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];

class _LinkMenu extends react__WEBPACK_IMPORTED_MODULE_5___default.a.PureComponent {
  getOptions() {
    const { props } = this;
    const { site, index, source, isPrivateBrowsingEnabled, siteInfo, platform } = props;

    // Handle special case of default site
    const propOptions = !site.isDefault ? props.options : DEFAULT_SITE_MENU_OPTIONS;

    const options = propOptions.map(o => content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_4__["LinkMenuOptions"][o](site, index, source, isPrivateBrowsingEnabled, siteInfo, platform)).map(option => {
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
            props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(userEventData));
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
    return react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_2__["ContextMenu"], {
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}

const getState = state => ({ isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled, platform: state.Prefs.values.platform });
const LinkMenu = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(getState)(Object(react_intl__WEBPACK_IMPORTED_MODULE_3__["injectIntl"])(_LinkMenu));

/***/ }),
/* 25 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ContextMenu", function() { return ContextMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ContextMenuItem", function() { return ContextMenuItem; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);


class ContextMenu extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
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
    return react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(
      "span",
      { className: "context-menu", onClick: this.onClick },
      react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(
        "ul",
        { role: "menu", className: "context-menu-list" },
        this.props.options.map((option, i) => option.type === "separator" ? react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("li", { key: i, className: "separator" }) : option.type !== "empty" && react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(ContextMenuItem, { key: i, option: option, hideContext: this.hideContext }))
      )
    );
  }
}

class ContextMenuItem extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
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
    return react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(
      "li",
      { role: "menuitem", className: "context-menu-item" },
      react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(
        "a",
        { onClick: this.onClick, onKeyDown: this.onKeyDown, tabIndex: "0", className: option.disabled ? "disabled" : "" },
        option.icon && react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", { className: `icon icon-spacer icon-${option.icon}` }),
        option.label
      )
    );
  }
}
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 26 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ScreenshotUtils", function() { return ScreenshotUtils; });
/**
 * List of helper functions for screenshot-based images.
 *
 * There are two kinds of images:
 * 1. Remote Image: This is the image from the main process and it refers to
 *    the image in the React props. This can either be an object with the `data`
 *    and `path` properties, if it is a blob, or a string, if it is a normal image.
 * 2. Local Image: This is the image object in the content process and it refers
 *    to the image *object* in the React component's state. All local image
 *    objects have the `url` property, and an additional property `path`, if they
 *    are blobs.
 */
const ScreenshotUtils = {
  isBlob(isLocal, image) {
    return !!(image && image.path && (!isLocal && image.data || isLocal && image.url));
  },

  // This should always be called with a remote image and not a local image.
  createLocalImageObject(remoteImage) {
    if (!remoteImage) {
      return null;
    }
    if (this.isBlob(false, remoteImage)) {
      return { url: global.URL.createObjectURL(remoteImage.data), path: remoteImage.path };
    }
    return { url: remoteImage };
  },

  // Revokes the object URL of the image if the local image is a blob.
  // This should always be called with a local image and not a remote image.
  maybeRevokeBlobObjectURL(localImage) {
    if (this.isBlob(true, localImage)) {
      global.URL.revokeObjectURL(localImage.url);
    }
  },

  // Checks if remoteImage and localImage are the same.
  isRemoteImageLocal(localImage, remoteImage) {
    // Both remoteImage and localImage are present.
    if (remoteImage && localImage) {
      return this.isBlob(false, remoteImage) ? localImage.path === remoteImage.path : localImage.url === remoteImage;
    }

    // This will only handle the remaining three possible outcomes.
    // (i.e. everything except when both image and localImage are present)
    return !remoteImage && !localImage;
  }
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 27 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Disclaimer", function() { return Disclaimer; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DisclaimerIntl", function() { return DisclaimerIntl; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_CollapsibleSection", function() { return _CollapsibleSection; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CollapsibleSection", function() { return CollapsibleSection; });
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);
/* harmony import */ var content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(17);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var content_src_components_SectionMenu_SectionMenu__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(28);
/* harmony import */ var content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(29);







const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

function getFormattedMessage(message) {
  return typeof message === "string" ? react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
    "span",
    null,
    message
  ) : react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], message);
}

class Disclaimer extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onAcknowledge = this.onAcknowledge.bind(this);
  }

  onAcknowledge() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].SetPref(this.props.disclaimerPref, false));
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({ event: "DISCLAIMER_ACKED", source: this.props.eventSource }));
  }

  render() {
    const { disclaimer } = this.props;
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "div",
      { className: "section-disclaimer" },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "div",
        { className: "section-disclaimer-text" },
        getFormattedMessage(disclaimer.text),
        disclaimer.link && react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "a",
          { href: disclaimer.link.href, target: "_blank", rel: "noopener noreferrer" },
          getFormattedMessage(disclaimer.link.title || disclaimer.link)
        )
      ),
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "button",
        { onClick: this.onAcknowledge },
        getFormattedMessage(disclaimer.button)
      )
    );
  }
}

const DisclaimerIntl = Object(react_intl__WEBPACK_IMPORTED_MODULE_0__["injectIntl"])(Disclaimer);

class _CollapsibleSection extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
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
    const { action, userEvent } = content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_5__["SectionMenuOptions"].CheckCollapsed(this.props);
    this.props.dispatch(action);
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({
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
      return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", { className: "icon icon-small-spacer", style: { backgroundImage: `url('${icon}')` } });
    }
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", { className: `icon icon-small-spacer icon-${icon || "webextension"}` });
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
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "section",
      {
        className: `collapsible-section ${this.props.className}${enableAnimation ? " animation-enabled" : ""}${collapsed ? " collapsed" : ""}${active ? " active" : ""}`
        // Note: data-section-id is used for web extension api tests in mozilla central
        , "data-section-id": id },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "div",
        { className: "section-top-bar" },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "h3",
          { className: "section-title" },
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "span",
            { className: "click-target", onClick: this.onHeaderClick },
            this.renderIcon(),
            getFormattedMessage(title),
            isCollapsible && react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", { className: `collapsible-arrow icon ${collapsed ? "icon-arrowhead-forward-small" : "icon-arrowhead-down-small"}` })
          )
        ),
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "div",
          null,
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "button",
            {
              className: "context-menu-button icon",
              onClick: this.onMenuButtonClick,
              onMouseEnter: this.onMenuButtonMouseEnter,
              onMouseLeave: this.onMenuButtonMouseLeave },
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "span",
              { className: "sr-only" },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "section_context_menu_button_sr" })
            )
          ),
          showContextMenu && react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_SectionMenu_SectionMenu__WEBPACK_IMPORTED_MODULE_4__["SectionMenu"], {
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
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_2__["ErrorBoundary"],
        { className: "section-body-fallback" },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "div",
          {
            className: `section-body${isAnimating ? " animating" : ""}`,
            onTransitionEnd: this.onTransitionEnd,
            ref: this.onBodyMount,
            style: isAnimating && !collapsed ? { maxHeight } : null },
          needsDisclaimer && react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(DisclaimerIntl, { disclaimerPref: disclaimerPref, disclaimer: disclaimer, eventSource: eventSource, dispatch: this.props.dispatch }),
          this.props.children
        )
      )
    );
  }
}

_CollapsibleSection.defaultProps = {
  document: global.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden"
  },
  Prefs: { values: {} }
};

const CollapsibleSection = Object(react_intl__WEBPACK_IMPORTED_MODULE_0__["injectIntl"])(_CollapsibleSection);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 28 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_SectionMenu", function() { return _SectionMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionMenu", function() { return SectionMenu; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(25);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(29);






const DEFAULT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "RemoveSection", "CheckCollapsed", "Separator", "ManageSection"];
const WEBEXT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "CheckCollapsed", "Separator", "ManageWebExtension"];

class _SectionMenu extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
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

    const options = propOptions.map(o => content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_4__["SectionMenuOptions"][o](props)).map(option => {
      const { action, id, type, userEvent } = option;
      if (!type && id) {
        option.label = props.intl.formatMessage({ id });
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
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
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_1__["ContextMenu"], {
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}

const SectionMenu = Object(react_intl__WEBPACK_IMPORTED_MODULE_2__["injectIntl"])(_SectionMenu);

/***/ }),
/* 29 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionMenuOptions", function() { return SectionMenuOptions; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);


/**
 * List of functions that return items that can be included as menu options in a
 * SectionMenu. All functions take the section as the only parameter.
 */
const SectionMenuOptions = {
  Separator: () => ({ type: "separator" }),
  MoveUp: section => ({
    id: "section_menu_action_move_up",
    icon: "arrowhead-up",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SECTION_MOVE,
      data: { id: section.id, direction: -1 }
    }),
    userEvent: "MENU_MOVE_UP",
    disabled: !!section.isFirst
  }),
  MoveDown: section => ({
    id: "section_menu_action_move_down",
    icon: "arrowhead-down",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SECTION_MOVE,
      data: { id: section.id, direction: +1 }
    }),
    userEvent: "MENU_MOVE_DOWN",
    disabled: !!section.isLast
  }),
  RemoveSection: section => ({
    id: "section_menu_action_remove_section",
    icon: "dismiss",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].SetPref(section.showPrefName, false),
    userEvent: "MENU_REMOVE"
  }),
  CollapseSection: section => ({
    id: "section_menu_action_collapse_section",
    icon: "minimize",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].UPDATE_SECTION_PREFS, data: { id: section.id, value: { collapsed: true } } }),
    userEvent: "MENU_COLLAPSE"
  }),
  ExpandSection: section => ({
    id: "section_menu_action_expand_section",
    icon: "maximize",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].UPDATE_SECTION_PREFS, data: { id: section.id, value: { collapsed: false } } }),
    userEvent: "MENU_EXPAND"
  }),
  ManageSection: section => ({
    id: "section_menu_action_manage_section",
    icon: "settings",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SETTINGS_OPEN }),
    userEvent: "MENU_MANAGE"
  }),
  ManageWebExtension: section => ({
    id: "section_menu_action_manage_webext",
    icon: "settings",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_WEBEXT_SETTINGS, data: section.id })
  }),
  AddTopSite: section => ({
    id: "section_menu_action_add_topsite",
    icon: "add",
    action: { type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT, data: { index: -1 } },
    userEvent: "MENU_ADD_TOPSITE"
  }),
  PrivacyNotice: section => ({
    id: "section_menu_action_privacy_notice",
    icon: "info",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
      data: { url: section.privacyNoticeURL }
    }),
    userEvent: "MENU_PRIVACY_NOTICE"
  }),
  CheckCollapsed: section => section.collapsed ? SectionMenuOptions.ExpandSection(section) : SectionMenuOptions.CollapseSection(section)
};

/***/ }),
/* 30 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ComponentPerfTimer", function() { return ComponentPerfTimer; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var common_PerfService_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(31);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);




// Currently record only a fixed set of sections. This will prevent data
// from custom sections from showing up or from topstories.
const RECORDED_SECTIONS = ["highlights", "topsites"];

class ComponentPerfTimer extends react__WEBPACK_IMPORTED_MODULE_2___default.a.Component {
  constructor(props) {
    super(props);
    // Just for test dependency injection:
    this.perfSvc = this.props.perfSvc || common_PerfService_jsm__WEBPACK_IMPORTED_MODULE_1__["perfService"];

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
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
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

      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
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

/***/ }),
/* 31 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_PerfService", function() { return _PerfService; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "perfService", function() { return perfService; });
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
/* 32 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Topic", function() { return Topic; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Topics", function() { return Topics; });
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);



class Topic extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const { url, name } = this.props;
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "li",
      null,
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "a",
        { key: name, className: "topic-link", href: url },
        name
      )
    );
  }
}

class Topics extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const { topics, read_more_endpoint } = this.props;
    return react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
      "div",
      { className: "topic" },
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "span",
        null,
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "pocket_read_more" })
      ),
      react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "ul",
        null,
        topics && topics.map(t => react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(Topic, { key: t.name, url: t.url, name: t.name }))
      ),
      read_more_endpoint && react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(
        "a",
        { className: "topic-read-more", href: read_more_endpoint },
        react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "pocket_read_even_more" })
      )
    );
  }
}

/***/ }),
/* 33 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_TopSites", function() { return _TopSites; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSites", function() { return TopSites; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(34);
/* harmony import */ var content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(27);
/* harmony import */ var content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(30);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(41);
/* harmony import */ var _TopSiteForm__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(44);
/* harmony import */ var _TopSite__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(35);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };












function topSiteIconType(link) {
  if (link.customScreenshotURL) {
    return "custom_screenshot";
  }
  if (link.tippyTopIcon || link.faviconRef === "tippytop") {
    return "tippytop";
  }
  if (link.faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["MIN_RICH_FAVICON_SIZE"]) {
    return "rich_icon";
  }
  if (link.screenshot && link.faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["MIN_CORNER_FAVICON_SIZE"]) {
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

class _TopSites extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
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
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
      data: { topsites_icon_stats: topSitesIconsStats, topsites_pinned: topSitesPinned }
    }));
  }

  /**
   * Return the TopSites that are visible based on prefs and window width.
   */
  _getVisibleTopSites() {
    // We hide 2 sites per row when not in the wide layout.
    let sitesPerRow = common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_7__["TOP_SITES_MAX_SITES_PER_ROW"];
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
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
      event: "TOP_SITES_EDIT_CLOSE"
    }));
    this.props.dispatch({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_CANCEL_EDIT });
  }

  render() {
    const { props } = this;
    const { editForm } = props.TopSites;

    return react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
      content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__["ComponentPerfTimer"],
      { id: "topsites", initialized: props.TopSites.initialized, dispatch: props.dispatch },
      react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
        content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__["CollapsibleSection"],
        {
          className: "top-sites",
          icon: "topsites",
          id: "topsites",
          title: { id: "header_top_sites" },
          extraMenuOptions: ["AddTopSite"],
          showPrefName: "feeds.topsites",
          eventSource: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
          collapsed: props.TopSites.pref ? props.TopSites.pref.collapsed : undefined,
          isFirst: props.isFirst,
          isLast: props.isLast,
          dispatch: props.dispatch },
        react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_TopSite__WEBPACK_IMPORTED_MODULE_9__["TopSiteList"], { TopSites: props.TopSites, TopSitesRows: props.TopSitesRows, dispatch: props.dispatch, intl: props.intl, topSiteIconType: topSiteIconType }),
        react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
          "div",
          { className: "edit-topsites-wrapper" },
          editForm && react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
            "div",
            { className: "edit-topsites" },
            react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", { className: "modal-overlay", onClick: this.onFormClose }),
            react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(
              "div",
              { className: "modal" },
              react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_TopSiteForm__WEBPACK_IMPORTED_MODULE_8__["TopSiteForm"], _extends({
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

const TopSites = Object(react_redux__WEBPACK_IMPORTED_MODULE_4__["connect"])(state => ({
  TopSites: state.TopSites,
  Prefs: state.Prefs,
  TopSitesRows: state.Prefs.values.topSitesRows
}))(Object(react_intl__WEBPACK_IMPORTED_MODULE_5__["injectIntl"])(_TopSites));
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 34 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_SOURCE", function() { return TOP_SITES_SOURCE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_CONTEXT_MENU_OPTIONS", function() { return TOP_SITES_CONTEXT_MENU_OPTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MIN_RICH_FAVICON_SIZE", function() { return MIN_RICH_FAVICON_SIZE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MIN_CORNER_FAVICON_SIZE", function() { return MIN_CORNER_FAVICON_SIZE; });
const TOP_SITES_SOURCE = "TOP_SITES";
const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];
// minimum size necessary to show a rich icon instead of a screenshot
const MIN_RICH_FAVICON_SIZE = 96;
// minimum size necessary to show any icon in the top left corner with a screenshot
const MIN_CORNER_FAVICON_SIZE = 16;

/***/ }),
/* 35 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteLink", function() { return TopSiteLink; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSite", function() { return TopSite; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSitePlaceholder", function() { return TopSitePlaceholder; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_TopSiteList", function() { return _TopSiteList; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteList", function() { return TopSiteList; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(34);
/* harmony import */ var content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(24);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(26);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(41);
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };









class TopSiteLink extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = { screenshotImage: null };
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

  /**
   * Helper to obtain the next state based on nextProps and prevState.
   *
   * NOTE: Rename this method to getDerivedStateFromProps when we update React
   *       to >= 16.3. We will need to update tests as well. We cannot rename this
   *       method to getDerivedStateFromProps now because there is a mismatch in
   *       the React version that we are using for both testing and production.
   *       (i.e. react-test-render => "16.3.2", react => "16.2.0").
   *
   * See https://github.com/airbnb/enzyme/blob/master/packages/enzyme-adapter-react-16/package.json#L43.
   */
  static getNextStateFromProps(nextProps, prevState) {
    const { screenshot } = nextProps.link;
    const imageInState = content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].isRemoteImageLocal(prevState.screenshotImage, screenshot);
    if (imageInState) {
      return null;
    }

    // Since image was updated, attempt to revoke old image blob URL, if it exists.
    content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].maybeRevokeBlobObjectURL(prevState.screenshotImage);

    return { screenshotImage: content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].createLocalImageObject(screenshot) };
  }

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillMount() {
    const nextState = TopSiteLink.getNextStateFromProps(this.props, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillReceiveProps(nextProps) {
    const nextState = TopSiteLink.getNextStateFromProps(nextProps, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  componentWillUnmount() {
    content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].maybeRevokeBlobObjectURL(this.state.screenshotImage);
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
    let hasScreenshotImage = this.state.screenshotImage && this.state.screenshotImage.url;
    if (defaultStyle) {
      // force no styles (letter fallback) even if the link has imagery
      smallFaviconFallback = false;
    } else if (link.customScreenshotURL) {
      // assume high quality custom screenshot and use rich icon styles and class names
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: hasScreenshotImage ? `url(${this.state.screenshotImage.url})` : "none"
      };
    } else if (tippyTopIcon || faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["MIN_RICH_FAVICON_SIZE"]) {
      // styles and class names for top sites with rich icons
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon || link.favicon})`
      };
    } else {
      // styles and class names for top sites with screenshot + small icon in top left corner
      imageClassName = `screenshot${hasScreenshotImage ? " active" : ""}`;
      imageStyle = { backgroundImage: hasScreenshotImage ? `url(${this.state.screenshotImage.url})` : "none" };

      // only show a favicon in top left if it's greater than 16x16
      if (faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["MIN_CORNER_FAVICON_SIZE"]) {
        showSmallFavicon = true;
        smallFaviconStyle = { backgroundImage: `url(${link.favicon})` };
      } else if (hasScreenshotImage) {
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
    return react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
      "li",
      _extends({ className: topSiteOuterClassName, onDrop: this.onDragEvent, onDragOver: this.onDragEvent, onDragEnter: this.onDragEvent, onDragLeave: this.onDragEvent }, draggableProps),
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
        "div",
        { className: "top-site-inner" },
        react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
          "a",
          { href: link.url, onClick: onClick },
          react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
            "div",
            { className: "tile", "aria-hidden": true, "data-fallback": letterFallback },
            react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", { className: imageClassName, style: imageStyle }),
            showSmallFavicon && react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
              className: "top-site-icon default-icon",
              "data-fallback": smallFaviconFallback && letterFallback,
              style: smallFaviconStyle })
          ),
          react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
            "div",
            { className: `title ${link.isPinned ? "pinned" : ""}` },
            link.isPinned && react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", { className: "icon icon-pin-small" }),
            react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
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
TopSiteLink.defaultProps = {
  title: "",
  link: {},
  isDraggable: true
};

class TopSite extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
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
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(Object.assign({
      event,
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_SOURCE"],
      action_position: this.props.index
    }, this._getTelemetryInfo())));
  }

  onLinkClick(event) {
    this.userEvent("CLICK");

    // Specially handle a top site link click for "typed" frecency bonus as
    // specified as a property on the link.
    event.preventDefault();
    const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
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
    return react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
      TopSiteLink,
      _extends({}, props, { onClick: this.onLinkClick, onDragEvent: this.props.onDragEvent, className: `${props.className || ""}${isContextMenuOpen ? " active" : ""}`, title: title }),
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
        "div",
        null,
        react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
          "button",
          { className: "context-menu-button icon", onClick: this.onMenuButtonClick },
          react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
            "span",
            { className: "sr-only" },
            react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_1__["FormattedMessage"], { id: "context_menu_button_sr", values: { title } })
          )
        ),
        isContextMenuOpen && react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_3__["LinkMenu"], {
          dispatch: props.dispatch,
          index: props.index,
          onUpdate: this.onMenuUpdate,
          options: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_CONTEXT_MENU_OPTIONS"],
          site: link,
          siteInfo: this._getTelemetryInfo(),
          source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_SOURCE"] })
      )
    );
  }
}
TopSite.defaultProps = {
  link: {},
  onActivate() {}
};

class TopSitePlaceholder extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
  }

  onEditButtonClick() {
    this.props.dispatch({ type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT, data: { index: this.props.index } });
  }

  render() {
    return react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
      TopSiteLink,
      _extends({}, this.props, { className: `placeholder ${this.props.className || ""}`, isDraggable: false }),
      react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("button", { className: "context-menu-button edit-button icon",
        title: this.props.intl.formatMessage({ id: "edit_topsites_edit_button" }),
        onClick: this.onEditButtonClick })
    );
  }
}

class _TopSiteList extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
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
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event,
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_SOURCE"],
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
          this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
            type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_INSERT,
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
    topSites.length = this.props.TopSitesRows * common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_6__["TOP_SITES_MAX_SITES_PER_ROW"];
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
      topSitesUI.push(!link ? react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSitePlaceholder, _extends({}, slotProps, commonProps)) : react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSite, _extends({
        link: link,
        activeIndex: this.state.activeIndex,
        onActivate: this.onActivate
      }, slotProps, commonProps)));
    }
    return react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(
      "ul",
      { className: `top-sites-list${this.state.draggedSite ? " dnd-active" : ""}` },
      topSitesUI
    );
  }
}

const TopSiteList = Object(react_intl__WEBPACK_IMPORTED_MODULE_1__["injectIntl"])(_TopSiteList);

/***/ }),
/* 36 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_StartupOverlay", function() { return _StartupOverlay; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "StartupOverlay", function() { return StartupOverlay; });
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(13);
/* harmony import */ var react_intl__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_intl__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(16);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(5);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);





class _StartupOverlay extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onInputChange = this.onInputChange.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.clickSkip = this.clickSkip.bind(this);
    this.initScene = this.initScene.bind(this);
    this.removeOverlay = this.removeOverlay.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);

    this.state = {
      emailInput: "",
      overlayRemoved: false
    };
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
      this.setState({ overlayRemoved: true });
    }, 400);
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({ emailInput: e.target.value });
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onSubmit() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({ event: "SUBMIT_EMAIL" }));
    window.addEventListener("visibilitychange", this.removeOverlay);
  }

  clickSkip() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({ event: "SKIPPED_SIGNIN" }));
    this.removeOverlay();
  }

  onInputInvalid(e) {
    let error = e.target.previousSibling;
    error.classList.add("active");
    e.target.classList.add("invalid");
    e.preventDefault(); // Override built-in form validation popup
    e.target.focus();
  }

  render() {
    // When skipping the onboarding tour we show AS but we are still on
    // about:welcome, prop.isFirstrun is true and StartupOverlay is rendered
    if (this.state.overlayRemoved) {
      return null;
    }

    let termsLink = react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "a",
      { href: "https://accounts.firefox.com/legal/terms", target: "_blank", rel: "noopener noreferrer" },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_terms_of_service" })
    );
    let privacyLink = react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "a",
      { href: "https://accounts.firefox.com/legal/privacy", target: "_blank", rel: "noopener noreferrer" },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_privacy_notice" })
    );
    return react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
      "div",
      { className: `overlay-wrapper ${this.state.show ? "show " : ""}` },
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", { className: "background" }),
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
        "div",
        { className: "firstrun-scene" },
        react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
          "div",
          { className: "fxaccounts-container" },
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "div",
            { className: "firstrun-left-divider" },
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "h1",
              { className: "firstrun-title" },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_title" })
            ),
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "p",
              { className: "firstrun-content" },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_content" })
            ),
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "a",
              { className: "firstrun-link", href: "https://www.mozilla.org/firefox/features/sync/", target: "_blank", rel: "noopener noreferrer" },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_learn_more_link" })
            )
          ),
          react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
            "div",
            { className: "firstrun-sign-in" },
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "p",
              { className: "form-header" },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_form_header" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
                "span",
                { className: "sub-header" },
                react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_form_sub_header" })
              )
            ),
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "form",
              { method: "get", action: "https://accounts.firefox.com", target: "_blank", rel: "noopener noreferrer", onSubmit: this.onSubmit },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "service", type: "hidden", value: "sync" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "action", type: "hidden", value: "email" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "context", type: "hidden", value: "fx_desktop_v3" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "entrypoint", type: "hidden", value: "activity-stream-firstrun" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "utm_source", type: "hidden", value: "activity-stream" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { name: "utm_campaign", type: "hidden", value: "firstrun" }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
                "span",
                { className: "error" },
                this.props.intl.formatMessage({ id: "firstrun_invalid_input" })
              ),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", { className: "email-input", name: "email", type: "email", required: "true", onInvalid: this.onInputInvalid, placeholder: this.props.intl.formatMessage({ id: "firstrun_email_input_placeholder" }), onChange: this.onInputChange }),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
                "div",
                { className: "extra-links" },
                react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], {
                  id: "firstrun_extra_legal_links",
                  values: {
                    terms: termsLink,
                    privacy: privacyLink
                  } })
              ),
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
                "button",
                { className: "continue-button", type: "submit" },
                react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_continue_to_login" })
              )
            ),
            react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(
              "button",
              { className: "skip-button", disabled: !!this.state.emailInput, onClick: this.clickSkip },
              react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_intl__WEBPACK_IMPORTED_MODULE_0__["FormattedMessage"], { id: "firstrun_skip_login" })
            )
          )
        )
      )
    );
  }
}

const StartupOverlay = Object(react_redux__WEBPACK_IMPORTED_MODULE_2__["connect"])()(Object(react_intl__WEBPACK_IMPORTED_MODULE_0__["injectIntl"])(_StartupOverlay));

/***/ }),
/* 37 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DetectUserSessionStart", function() { return DetectUserSessionStart; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var common_PerfService_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(31);



const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

class DetectUserSessionStart {
  constructor(store, options = {}) {
    this._store = store;
    // Overrides for testing
    this.document = options.document || global.document;
    this._perfService = options.perfService || common_PerfService_jsm__WEBPACK_IMPORTED_MODULE_1__["perfService"];
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

      this._store.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
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
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(1)))

/***/ }),
/* 38 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// CONCATENATED MODULE: ./node_modules/fluent/src/parser.js
/*  eslint no-magic-numbers: [0]  */

const MAX_PLACEABLES = 100;

const entryIdentifierRe = /-?[a-zA-Z][a-zA-Z0-9_-]*/y;
const identifierRe = /[a-zA-Z][a-zA-Z0-9_-]*/y;
const functionIdentifierRe = /^[A-Z][A-Z_?-]*$/;

/**
 * The `Parser` class is responsible for parsing FTL resources.
 *
 * It's only public method is `getResource(source)` which takes an FTL string
 * and returns a two element Array with an Object of entries generated from the
 * source as the first element and an array of SyntaxError objects as the
 * second.
 *
 * This parser is optimized for runtime performance.
 *
 * There is an equivalent of this parser in syntax/parser which is
 * generating full AST which is useful for FTL tools.
 */
class RuntimeParser {
  /**
   * Parse FTL code into entries formattable by the MessageContext.
   *
   * Given a string of FTL syntax, return a map of entries that can be passed
   * to MessageContext.format and a list of errors encountered during parsing.
   *
   * @param {String} string
   * @returns {Array<Object, Array>}
   */
  getResource(string) {
    this._source = string;
    this._index = 0;
    this._length = string.length;
    this.entries = {};

    const errors = [];

    this.skipWS();
    while (this._index < this._length) {
      try {
        this.getEntry();
      } catch (e) {
        if (e instanceof SyntaxError) {
          errors.push(e);

          this.skipToNextEntryStart();
        } else {
          throw e;
        }
      }
      this.skipWS();
    }

    return [this.entries, errors];
  }

  /**
   * Parse the source string from the current index as an FTL entry
   * and add it to object's entries property.
   *
   * @private
   */
  getEntry() {
    // The index here should either be at the beginning of the file
    // or right after new line.
    if (this._index !== 0 && this._source[this._index - 1] !== "\n") {
      throw this.error(`Expected an entry to start
        at the beginning of the file or on a new line.`);
    }

    const ch = this._source[this._index];

    // We don't care about comments or sections at runtime
    if (ch === "/" || ch === "#" && [" ", "#", "\n"].includes(this._source[this._index + 1])) {
      this.skipComment();
      return;
    }

    if (ch === "[") {
      this.skipSection();
      return;
    }

    this.getMessage();
  }

  /**
   * Skip the section entry from the current index.
   *
   * @private
   */
  skipSection() {
    this._index += 1;
    if (this._source[this._index] !== "[") {
      throw this.error('Expected "[[" to open a section');
    }

    this._index += 1;

    this.skipInlineWS();
    this.getVariantName();
    this.skipInlineWS();

    if (this._source[this._index] !== "]" || this._source[this._index + 1] !== "]") {
      throw this.error('Expected "]]" to close a section');
    }

    this._index += 2;
  }

  /**
   * Parse the source string from the current index as an FTL message
   * and add it to the entries property on the Parser.
   *
   * @private
   */
  getMessage() {
    const id = this.getEntryIdentifier();

    this.skipInlineWS();

    if (this._source[this._index] === "=") {
      this._index++;
    }

    this.skipInlineWS();

    const val = this.getPattern();

    if (id.startsWith("-") && val === null) {
      throw this.error("Expected term to have a value");
    }

    let attrs = null;

    if (this._source[this._index] === " ") {
      const lineStart = this._index;
      this.skipInlineWS();

      if (this._source[this._index] === ".") {
        this._index = lineStart;
        attrs = this.getAttributes();
      }
    }

    if (attrs === null && typeof val === "string") {
      this.entries[id] = val;
    } else {
      if (val === null && attrs === null) {
        throw this.error("Expected message to have a value or attributes");
      }

      this.entries[id] = {};

      if (val !== null) {
        this.entries[id].val = val;
      }

      if (attrs !== null) {
        this.entries[id].attrs = attrs;
      }
    }
  }

  /**
   * Skip whitespace.
   *
   * @private
   */
  skipWS() {
    let ch = this._source[this._index];
    while (ch === " " || ch === "\n" || ch === "\t" || ch === "\r") {
      ch = this._source[++this._index];
    }
  }

  /**
   * Skip inline whitespace (space and \t).
   *
   * @private
   */
  skipInlineWS() {
    let ch = this._source[this._index];
    while (ch === " " || ch === "\t") {
      ch = this._source[++this._index];
    }
  }

  /**
   * Skip blank lines.
   *
   * @private
   */
  skipBlankLines() {
    while (true) {
      const ptr = this._index;

      this.skipInlineWS();

      if (this._source[this._index] === "\n") {
        this._index += 1;
      } else {
        this._index = ptr;
        break;
      }
    }
  }

  /**
   * Get identifier using the provided regex.
   *
   * By default this will get identifiers of public messages, attributes and
   * external arguments (without the $).
   *
   * @returns {String}
   * @private
   */
  getIdentifier(re = identifierRe) {
    re.lastIndex = this._index;
    const result = re.exec(this._source);

    if (result === null) {
      this._index += 1;
      throw this.error(`Expected an identifier [${re.toString()}]`);
    }

    this._index = re.lastIndex;
    return result[0];
  }

  /**
   * Get identifier of a Message or a Term (staring with a dash).
   *
   * @returns {String}
   * @private
   */
  getEntryIdentifier() {
    return this.getIdentifier(entryIdentifierRe);
  }

  /**
   * Get Variant name.
   *
   * @returns {Object}
   * @private
   */
  getVariantName() {
    let name = "";

    const start = this._index;
    let cc = this._source.charCodeAt(this._index);

    if (cc >= 97 && cc <= 122 || // a-z
    cc >= 65 && cc <= 90 || // A-Z
    cc === 95 || cc === 32) {
      // _ <space>
      cc = this._source.charCodeAt(++this._index);
    } else {
      throw this.error("Expected a keyword (starting with [a-zA-Z_])");
    }

    while (cc >= 97 && cc <= 122 || // a-z
    cc >= 65 && cc <= 90 || // A-Z
    cc >= 48 && cc <= 57 || // 0-9
    cc === 95 || cc === 45 || cc === 32) {
      // _- <space>
      cc = this._source.charCodeAt(++this._index);
    }

    // If we encountered the end of name, we want to test if the last
    // collected character is a space.
    // If it is, we will backtrack to the last non-space character because
    // the keyword cannot end with a space character.
    while (this._source.charCodeAt(this._index - 1) === 32) {
      this._index--;
    }

    name += this._source.slice(start, this._index);

    return { type: "varname", name };
  }

  /**
   * Get simple string argument enclosed in `"`.
   *
   * @returns {String}
   * @private
   */
  getString() {
    const start = this._index + 1;

    while (++this._index < this._length) {
      const ch = this._source[this._index];

      if (ch === '"') {
        break;
      }

      if (ch === "\n") {
        throw this.error("Unterminated string expression");
      }
    }

    return this._source.substring(start, this._index++);
  }

  /**
   * Parses a Message pattern.
   * Message Pattern may be a simple string or an array of strings
   * and placeable expressions.
   *
   * @returns {String|Array}
   * @private
   */
  getPattern() {
    // We're going to first try to see if the pattern is simple.
    // If it is we can just look for the end of the line and read the string.
    //
    // Then, if either the line contains a placeable opening `{` or the
    // next line starts an indentation, we switch to complex pattern.
    const start = this._index;
    let eol = this._source.indexOf("\n", this._index);

    if (eol === -1) {
      eol = this._length;
    }

    const firstLineContent = start !== eol ? this._source.slice(start, eol) : null;

    if (firstLineContent && firstLineContent.includes("{")) {
      return this.getComplexPattern();
    }

    this._index = eol + 1;

    this.skipBlankLines();

    if (this._source[this._index] !== " ") {
      // No indentation means we're done with this message. Callers should check
      // if the return value here is null. It may be OK for messages, but not OK
      // for terms, attributes and variants.
      return firstLineContent;
    }

    const lineStart = this._index;

    this.skipInlineWS();

    if (this._source[this._index] === ".") {
      // The pattern is followed by an attribute. Rewind _index to the first
      // column of the current line as expected by getAttributes.
      this._index = lineStart;
      return firstLineContent;
    }

    if (firstLineContent) {
      // It's a multiline pattern which started on the same line as the
      // identifier. Reparse the whole pattern to make sure we get all of it.
      this._index = start;
    }

    return this.getComplexPattern();
  }

  /**
   * Parses a complex Message pattern.
   * This function is called by getPattern when the message is multiline,
   * or contains escape chars or placeables.
   * It does full parsing of complex patterns.
   *
   * @returns {Array}
   * @private
   */
  /* eslint-disable complexity */
  getComplexPattern() {
    let buffer = "";
    const content = [];
    let placeables = 0;

    let ch = this._source[this._index];

    while (this._index < this._length) {
      // This block handles multi-line strings combining strings separated
      // by new line.
      if (ch === "\n") {
        this._index++;

        // We want to capture the start and end pointers
        // around blank lines and add them to the buffer
        // but only if the blank lines are in the middle
        // of the string.
        const blankLinesStart = this._index;
        this.skipBlankLines();
        const blankLinesEnd = this._index;

        if (this._source[this._index] !== " ") {
          break;
        }
        this.skipInlineWS();

        if (this._source[this._index] === "}" || this._source[this._index] === "[" || this._source[this._index] === "*" || this._source[this._index] === ".") {
          this._index = blankLinesEnd;
          break;
        }

        buffer += this._source.substring(blankLinesStart, blankLinesEnd);

        if (buffer.length || content.length) {
          buffer += "\n";
        }
        ch = this._source[this._index];
        continue;
      } else if (ch === "\\") {
        const ch2 = this._source[this._index + 1];
        if (ch2 === '"' || ch2 === "{" || ch2 === "\\") {
          ch = ch2;
          this._index++;
        }
      } else if (ch === "{") {
        // Push the buffer to content array right before placeable
        if (buffer.length) {
          content.push(buffer);
        }
        if (placeables > MAX_PLACEABLES - 1) {
          throw this.error(`Too many placeables, maximum allowed is ${MAX_PLACEABLES}`);
        }
        buffer = "";
        content.push(this.getPlaceable());

        this._index++;

        ch = this._source[this._index];
        placeables++;
        continue;
      }

      if (ch) {
        buffer += ch;
      }
      this._index++;
      ch = this._source[this._index];
    }

    if (content.length === 0) {
      return buffer.length ? buffer : null;
    }

    if (buffer.length) {
      content.push(buffer);
    }

    return content;
  }
  /* eslint-enable complexity */

  /**
   * Parses a single placeable in a Message pattern and returns its
   * expression.
   *
   * @returns {Object}
   * @private
   */
  getPlaceable() {
    const start = ++this._index;

    this.skipWS();

    if (this._source[this._index] === "*" || this._source[this._index] === "[" && this._source[this._index + 1] !== "]") {
      const variants = this.getVariants();

      return {
        type: "sel",
        exp: null,
        vars: variants[0],
        def: variants[1]
      };
    }

    // Rewind the index and only support in-line white-space now.
    this._index = start;
    this.skipInlineWS();

    const selector = this.getSelectorExpression();

    this.skipWS();

    const ch = this._source[this._index];

    if (ch === "}") {
      if (selector.type === "attr" && selector.id.name.startsWith("-")) {
        throw this.error("Attributes of private messages cannot be interpolated.");
      }

      return selector;
    }

    if (ch !== "-" || this._source[this._index + 1] !== ">") {
      throw this.error('Expected "}" or "->"');
    }

    if (selector.type === "ref") {
      throw this.error("Message references cannot be used as selectors.");
    }

    if (selector.type === "var") {
      throw this.error("Variants cannot be used as selectors.");
    }

    if (selector.type === "attr" && !selector.id.name.startsWith("-")) {
      throw this.error("Attributes of public messages cannot be used as selectors.");
    }

    this._index += 2; // ->

    this.skipInlineWS();

    if (this._source[this._index] !== "\n") {
      throw this.error("Variants should be listed in a new line");
    }

    this.skipWS();

    const variants = this.getVariants();

    if (variants[0].length === 0) {
      throw this.error("Expected members for the select expression");
    }

    return {
      type: "sel",
      exp: selector,
      vars: variants[0],
      def: variants[1]
    };
  }

  /**
   * Parses a selector expression.
   *
   * @returns {Object}
   * @private
   */
  getSelectorExpression() {
    const literal = this.getLiteral();

    if (literal.type !== "ref") {
      return literal;
    }

    if (this._source[this._index] === ".") {
      this._index++;

      const name = this.getIdentifier();
      this._index++;
      return {
        type: "attr",
        id: literal,
        name
      };
    }

    if (this._source[this._index] === "[") {
      this._index++;

      const key = this.getVariantKey();
      this._index++;
      return {
        type: "var",
        id: literal,
        key
      };
    }

    if (this._source[this._index] === "(") {
      this._index++;
      const args = this.getCallArgs();

      if (!functionIdentifierRe.test(literal.name)) {
        throw this.error("Function names must be all upper-case");
      }

      this._index++;

      literal.type = "fun";

      return {
        type: "call",
        fun: literal,
        args
      };
    }

    return literal;
  }

  /**
   * Parses call arguments for a CallExpression.
   *
   * @returns {Array}
   * @private
   */
  getCallArgs() {
    const args = [];

    while (this._index < this._length) {
      this.skipInlineWS();

      if (this._source[this._index] === ")") {
        return args;
      }

      const exp = this.getSelectorExpression();

      // MessageReference in this place may be an entity reference, like:
      // `call(foo)`, or, if it's followed by `:` it will be a key-value pair.
      if (exp.type !== "ref") {
        args.push(exp);
      } else {
        this.skipInlineWS();

        if (this._source[this._index] === ":") {
          this._index++;
          this.skipInlineWS();

          const val = this.getSelectorExpression();

          // If the expression returned as a value of the argument
          // is not a quote delimited string or number, throw.
          //
          // We don't have to check here if the pattern is quote delimited
          // because that's the only type of string allowed in expressions.
          if (typeof val === "string" || Array.isArray(val) || val.type === "num") {
            args.push({
              type: "narg",
              name: exp.name,
              val
            });
          } else {
            this._index = this._source.lastIndexOf(":", this._index) + 1;
            throw this.error("Expected string in quotes, number.");
          }
        } else {
          args.push(exp);
        }
      }

      this.skipInlineWS();

      if (this._source[this._index] === ")") {
        break;
      } else if (this._source[this._index] === ",") {
        this._index++;
      } else {
        throw this.error('Expected "," or ")"');
      }
    }

    return args;
  }

  /**
   * Parses an FTL Number.
   *
   * @returns {Object}
   * @private
   */
  getNumber() {
    let num = "";
    let cc = this._source.charCodeAt(this._index);

    // The number literal may start with negative sign `-`.
    if (cc === 45) {
      num += "-";
      cc = this._source.charCodeAt(++this._index);
    }

    // next, we expect at least one digit
    if (cc < 48 || cc > 57) {
      throw this.error(`Unknown literal "${num}"`);
    }

    // followed by potentially more digits
    while (cc >= 48 && cc <= 57) {
      num += this._source[this._index++];
      cc = this._source.charCodeAt(this._index);
    }

    // followed by an optional decimal separator `.`
    if (cc === 46) {
      num += this._source[this._index++];
      cc = this._source.charCodeAt(this._index);

      // followed by at least one digit
      if (cc < 48 || cc > 57) {
        throw this.error(`Unknown literal "${num}"`);
      }

      // and optionally more digits
      while (cc >= 48 && cc <= 57) {
        num += this._source[this._index++];
        cc = this._source.charCodeAt(this._index);
      }
    }

    return {
      type: "num",
      val: num
    };
  }

  /**
   * Parses a list of Message attributes.
   *
   * @returns {Object}
   * @private
   */
  getAttributes() {
    const attrs = {};

    while (this._index < this._length) {
      if (this._source[this._index] !== " ") {
        break;
      }
      this.skipInlineWS();

      if (this._source[this._index] !== ".") {
        break;
      }
      this._index++;

      const key = this.getIdentifier();

      this.skipInlineWS();

      if (this._source[this._index] !== "=") {
        throw this.error('Expected "="');
      }
      this._index++;

      this.skipInlineWS();

      const val = this.getPattern();

      if (val === null) {
        throw this.error("Expected attribute to have a value");
      }

      if (typeof val === "string") {
        attrs[key] = val;
      } else {
        attrs[key] = {
          val
        };
      }

      this.skipBlankLines();
    }

    return attrs;
  }

  /**
   * Parses a list of Selector variants.
   *
   * @returns {Array}
   * @private
   */
  getVariants() {
    const variants = [];
    let index = 0;
    let defaultIndex;

    while (this._index < this._length) {
      const ch = this._source[this._index];

      if ((ch !== "[" || this._source[this._index + 1] === "[") && ch !== "*") {
        break;
      }
      if (ch === "*") {
        this._index++;
        defaultIndex = index;
      }

      if (this._source[this._index] !== "[") {
        throw this.error('Expected "["');
      }

      this._index++;

      const key = this.getVariantKey();

      this.skipInlineWS();

      const val = this.getPattern();

      if (val === null) {
        throw this.error("Expected variant to have a value");
      }

      variants[index++] = { key, val };

      this.skipWS();
    }

    return [variants, defaultIndex];
  }

  /**
   * Parses a Variant key.
   *
   * @returns {String}
   * @private
   */
  getVariantKey() {
    // VariantKey may be a Keyword or Number

    const cc = this._source.charCodeAt(this._index);
    let literal;

    if (cc >= 48 && cc <= 57 || cc === 45) {
      literal = this.getNumber();
    } else {
      literal = this.getVariantName();
    }

    if (this._source[this._index] !== "]") {
      throw this.error('Expected "]"');
    }

    this._index++;
    return literal;
  }

  /**
   * Parses an FTL literal.
   *
   * @returns {Object}
   * @private
   */
  getLiteral() {
    const cc0 = this._source.charCodeAt(this._index);

    if (cc0 === 36) {
      // $
      this._index++;
      return {
        type: "ext",
        name: this.getIdentifier()
      };
    }

    const cc1 = cc0 === 45 // -
    // Peek at the next character after the dash.
    ? this._source.charCodeAt(this._index + 1)
    // Or keep using the character at the current index.
    : cc0;

    if (cc1 >= 97 && cc1 <= 122 || // a-z
    cc1 >= 65 && cc1 <= 90) {
      // A-Z
      return {
        type: "ref",
        name: this.getEntryIdentifier()
      };
    }

    if (cc1 >= 48 && cc1 <= 57) {
      // 0-9
      return this.getNumber();
    }

    if (cc0 === 34) {
      // "
      return this.getString();
    }

    throw this.error("Expected literal");
  }

  /**
   * Skips an FTL comment.
   *
   * @private
   */
  skipComment() {
    // At runtime, we don't care about comments so we just have
    // to parse them properly and skip their content.
    let eol = this._source.indexOf("\n", this._index);

    while (eol !== -1 && (this._source[eol + 1] === "/" && this._source[eol + 2] === "/" || this._source[eol + 1] === "#" && [" ", "#"].includes(this._source[eol + 2]))) {
      this._index = eol + 3;

      eol = this._source.indexOf("\n", this._index);

      if (eol === -1) {
        break;
      }
    }

    if (eol === -1) {
      this._index = this._length;
    } else {
      this._index = eol + 1;
    }
  }

  /**
   * Creates a new SyntaxError object with a given message.
   *
   * @param {String} message
   * @returns {Object}
   * @private
   */
  error(message) {
    return new SyntaxError(message);
  }

  /**
   * Skips to the beginning of a next entry after the current position.
   * This is used to mark the boundary of junk entry in case of error,
   * and recover from the returned position.
   *
   * @private
   */
  skipToNextEntryStart() {
    let start = this._index;

    while (true) {
      if (start === 0 || this._source[start - 1] === "\n") {
        const cc = this._source.charCodeAt(start);

        if (cc >= 97 && cc <= 122 || // a-z
        cc >= 65 && cc <= 90 || // A-Z
        cc === 47 || cc === 91) {
          // /[
          this._index = start;
          return;
        }
      }

      start = this._source.indexOf("\n", start);

      if (start === -1) {
        this._index = this._length;
        return;
      }
      start++;
    }
  }
}

/**
 * Parses an FTL string using RuntimeParser and returns the generated
 * object with entries and a list of errors.
 *
 * @param {String} string
 * @returns {Array<Object, Array>}
 */
function parse(string) {
  const parser = new RuntimeParser();
  return parser.getResource(string);
}
// CONCATENATED MODULE: ./node_modules/fluent/src/types.js
/* global Intl */

/**
 * The `FluentType` class is the base of Fluent's type system.
 *
 * Fluent types wrap JavaScript values and store additional configuration for
 * them, which can then be used in the `toString` method together with a proper
 * `Intl` formatter.
 */
class FluentType {

  /**
   * Create an `FluentType` instance.
   *
   * @param   {Any}    value - JavaScript value to wrap.
   * @param   {Object} opts  - Configuration.
   * @returns {FluentType}
   */
  constructor(value, opts) {
    this.value = value;
    this.opts = opts;
  }

  /**
   * Unwrap the raw value stored by this `FluentType`.
   *
   * @returns {Any}
   */
  valueOf() {
    return this.value;
  }

  /**
   * Format this instance of `FluentType` to a string.
   *
   * Formatted values are suitable for use outside of the `MessageContext`.
   * This method can use `Intl` formatters memoized by the `MessageContext`
   * instance passed as an argument.
   *
   * @param   {MessageContext} [ctx]
   * @returns {string}
   */
  toString() {
    throw new Error("Subclasses of FluentType must implement toString.");
  }
}

class FluentNone extends FluentType {
  toString() {
    return this.value || "???";
  }
}

class FluentNumber extends FluentType {
  constructor(value, opts) {
    super(parseFloat(value), opts);
  }

  toString(ctx) {
    try {
      const nf = ctx._memoizeIntlObject(Intl.NumberFormat, this.opts);
      return nf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
  }

  /**
   * Compare the object with another instance of a FluentType.
   *
   * @param   {MessageContext} ctx
   * @param   {FluentType}     other
   * @returns {bool}
   */
  match(ctx, other) {
    if (other instanceof FluentNumber) {
      return this.value === other.value;
    }
    return false;
  }
}

class FluentDateTime extends FluentType {
  constructor(value, opts) {
    super(new Date(value), opts);
  }

  toString(ctx) {
    try {
      const dtf = ctx._memoizeIntlObject(Intl.DateTimeFormat, this.opts);
      return dtf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
  }
}

class FluentSymbol extends FluentType {
  toString() {
    return this.value;
  }

  /**
   * Compare the object with another instance of a FluentType.
   *
   * @param   {MessageContext} ctx
   * @param   {FluentType}     other
   * @returns {bool}
   */
  match(ctx, other) {
    if (other instanceof FluentSymbol) {
      return this.value === other.value;
    } else if (typeof other === "string") {
      return this.value === other;
    } else if (other instanceof FluentNumber) {
      const pr = ctx._memoizeIntlObject(Intl.PluralRules, other.opts);
      return this.value === pr.select(other.value);
    }
    return false;
  }
}
// CONCATENATED MODULE: ./node_modules/fluent/src/builtins.js
/**
 * @overview
 *
 * The FTL resolver ships with a number of functions built-in.
 *
 * Each function take two arguments:
 *   - args - an array of positional args
 *   - opts - an object of key-value args
 *
 * Arguments to functions are guaranteed to already be instances of
 * `FluentType`.  Functions must return `FluentType` objects as well.
 */



/* harmony default export */ var builtins = ({
  "NUMBER": ([arg], opts) => new FluentNumber(arg.valueOf(), merge(arg.opts, opts)),
  "DATETIME": ([arg], opts) => new FluentDateTime(arg.valueOf(), merge(arg.opts, opts))
});

function merge(argopts, opts) {
  return Object.assign({}, argopts, values(opts));
}

function values(opts) {
  const unwrapped = {};
  for (const [name, opt] of Object.entries(opts)) {
    unwrapped[name] = opt.valueOf();
  }
  return unwrapped;
}
// CONCATENATED MODULE: ./node_modules/fluent/src/resolver.js
/**
 * @overview
 *
 * The role of the Fluent resolver is to format a translation object to an
 * instance of `FluentType` or an array of instances.
 *
 * Translations can contain references to other messages or external arguments,
 * conditional logic in form of select expressions, traits which describe their
 * grammatical features, and can use Fluent builtins which make use of the
 * `Intl` formatters to format numbers, dates, lists and more into the
 * context's language.  See the documentation of the Fluent syntax for more
 * information.
 *
 * In case of errors the resolver will try to salvage as much of the
 * translation as possible.  In rare situations where the resolver didn't know
 * how to recover from an error it will return an instance of `FluentNone`.
 *
 * `MessageReference`, `VariantExpression`, `AttributeExpression` and
 * `SelectExpression` resolve to raw Runtime Entries objects and the result of
 * the resolution needs to be passed into `Type` to get their real value.
 * This is useful for composing expressions.  Consider:
 *
 *     brand-name[nominative]
 *
 * which is a `VariantExpression` with properties `id: MessageReference` and
 * `key: Keyword`.  If `MessageReference` was resolved eagerly, it would
 * instantly resolve to the value of the `brand-name` message.  Instead, we
 * want to get the message object and look for its `nominative` variant.
 *
 * All other expressions (except for `FunctionReference` which is only used in
 * `CallExpression`) resolve to an instance of `FluentType`.  The caller should
 * use the `toString` method to convert the instance to a native value.
 *
 *
 * All functions in this file pass around a special object called `env`.
 * This object stores a set of elements used by all resolve functions:
 *
 *  * {MessageContext} ctx
 *      context for which the given resolution is happening
 *  * {Object} args
 *      list of developer provided arguments that can be used
 *  * {Array} errors
 *      list of errors collected while resolving
 *  * {WeakSet} dirty
 *      Set of patterns already encountered during this resolution.
 *      This is used to prevent cyclic resolutions.
 */




// Prevent expansion of too long placeables.
const MAX_PLACEABLE_LENGTH = 2500;

// Unicode bidi isolation characters.
const FSI = "\u2068";
const PDI = "\u2069";

/**
 * Helper for choosing the default value from a set of members.
 *
 * Used in SelectExpressions and Type.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} members
 *    Hash map of variants from which the default value is to be selected.
 * @param   {Number} def
 *    The index of the default variant.
 * @returns {FluentType}
 * @private
 */
function DefaultMember(env, members, def) {
  if (members[def]) {
    return members[def];
  }

  const { errors } = env;
  errors.push(new RangeError("No default"));
  return new FluentNone();
}

/**
 * Resolve a reference to another message.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} id
 *    The identifier of the message to be resolved.
 * @param   {String} id.name
 *    The name of the identifier.
 * @returns {FluentType}
 * @private
 */
function MessageReference(env, { name }) {
  const { ctx, errors } = env;
  const message = name.startsWith("-") ? ctx._terms.get(name) : ctx._messages.get(name);

  if (!message) {
    const err = name.startsWith("-") ? new ReferenceError(`Unknown term: ${name}`) : new ReferenceError(`Unknown message: ${name}`);
    errors.push(err);
    return new FluentNone(name);
  }

  return message;
}

/**
 * Resolve a variant expression to the variant object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {Object} expr.id
 *    An Identifier of a message for which the variant is resolved.
 * @param   {Object} expr.id.name
 *    Name a message for which the variant is resolved.
 * @param   {Object} expr.key
 *    Variant key to be resolved.
 * @returns {FluentType}
 * @private
 */
function VariantExpression(env, { id, key }) {
  const message = MessageReference(env, id);
  if (message instanceof FluentNone) {
    return message;
  }

  const { ctx, errors } = env;
  const keyword = Type(env, key);

  function isVariantList(node) {
    return Array.isArray(node) && node[0].type === "sel" && node[0].exp === null;
  }

  if (isVariantList(message.val)) {
    // Match the specified key against keys of each variant, in order.
    for (const variant of message.val[0].vars) {
      const variantKey = Type(env, variant.key);
      if (keyword.match(ctx, variantKey)) {
        return variant;
      }
    }
  }

  errors.push(new ReferenceError(`Unknown variant: ${keyword.toString(ctx)}`));
  return Type(env, message);
}

/**
 * Resolve an attribute expression to the attribute object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.id
 *    An ID of a message for which the attribute is resolved.
 * @param   {String} expr.name
 *    Name of the attribute to be resolved.
 * @returns {FluentType}
 * @private
 */
function AttributeExpression(env, { id, name }) {
  const message = MessageReference(env, id);
  if (message instanceof FluentNone) {
    return message;
  }

  if (message.attrs) {
    // Match the specified name against keys of each attribute.
    for (const attrName in message.attrs) {
      if (name === attrName) {
        return message.attrs[name];
      }
    }
  }

  const { errors } = env;
  errors.push(new ReferenceError(`Unknown attribute: ${name}`));
  return Type(env, message);
}

/**
 * Resolve a select expression to the member object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.exp
 *    Selector expression
 * @param   {Array} expr.vars
 *    List of variants for the select expression.
 * @param   {Number} expr.def
 *    Index of the default variant.
 * @returns {FluentType}
 * @private
 */
function SelectExpression(env, { exp, vars, def }) {
  if (exp === null) {
    return DefaultMember(env, vars, def);
  }

  const selector = Type(env, exp);
  if (selector instanceof FluentNone) {
    return DefaultMember(env, vars, def);
  }

  // Match the selector against keys of each variant, in order.
  for (const variant of vars) {
    const key = Type(env, variant.key);
    const keyCanMatch = key instanceof FluentNumber || key instanceof FluentSymbol;

    if (!keyCanMatch) {
      continue;
    }

    const { ctx } = env;

    if (key.match(ctx, selector)) {
      return variant;
    }
  }

  return DefaultMember(env, vars, def);
}

/**
 * Resolve expression to a Fluent type.
 *
 * JavaScript strings are a special case.  Since they natively have the
 * `toString` method they can be used as if they were a Fluent type without
 * paying the cost of creating a instance of one.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression object to be resolved into a Fluent type.
 * @returns {FluentType}
 * @private
 */
function Type(env, expr) {
  // A fast-path for strings which are the most common case, and for
  // `FluentNone` which doesn't require any additional logic.
  if (typeof expr === "string" || expr instanceof FluentNone) {
    return expr;
  }

  // The Runtime AST (Entries) encodes patterns (complex strings with
  // placeables) as Arrays.
  if (Array.isArray(expr)) {
    return Pattern(env, expr);
  }

  switch (expr.type) {
    case "varname":
      return new FluentSymbol(expr.name);
    case "num":
      return new FluentNumber(expr.val);
    case "ext":
      return ExternalArgument(env, expr);
    case "fun":
      return FunctionReference(env, expr);
    case "call":
      return CallExpression(env, expr);
    case "ref":
      {
        const message = MessageReference(env, expr);
        return Type(env, message);
      }
    case "attr":
      {
        const attr = AttributeExpression(env, expr);
        return Type(env, attr);
      }
    case "var":
      {
        const variant = VariantExpression(env, expr);
        return Type(env, variant);
      }
    case "sel":
      {
        const member = SelectExpression(env, expr);
        return Type(env, member);
      }
    case undefined:
      {
        // If it's a node with a value, resolve the value.
        if (expr.val !== null && expr.val !== undefined) {
          return Type(env, expr.val);
        }

        const { errors } = env;
        errors.push(new RangeError("No value"));
        return new FluentNone();
      }
    default:
      return new FluentNone();
  }
}

/**
 * Resolve a reference to an external argument.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.name
 *    Name of an argument to be returned.
 * @returns {FluentType}
 * @private
 */
function ExternalArgument(env, { name }) {
  const { args, errors } = env;

  if (!args || !args.hasOwnProperty(name)) {
    errors.push(new ReferenceError(`Unknown external: ${name}`));
    return new FluentNone(name);
  }

  const arg = args[name];

  // Return early if the argument already is an instance of FluentType.
  if (arg instanceof FluentType) {
    return arg;
  }

  // Convert the argument to a Fluent type.
  switch (typeof arg) {
    case "string":
      return arg;
    case "number":
      return new FluentNumber(arg);
    case "object":
      if (arg instanceof Date) {
        return new FluentDateTime(arg);
      }
    default:
      errors.push(new TypeError(`Unsupported external type: ${name}, ${typeof arg}`));
      return new FluentNone(name);
  }
}

/**
 * Resolve a reference to a function.
 *
 * @param   {Object}  env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.name
 *    Name of the function to be returned.
 * @returns {Function}
 * @private
 */
function FunctionReference(env, { name }) {
  // Some functions are built-in.  Others may be provided by the runtime via
  // the `MessageContext` constructor.
  const { ctx: { _functions }, errors } = env;
  const func = _functions[name] || builtins[name];

  if (!func) {
    errors.push(new ReferenceError(`Unknown function: ${name}()`));
    return new FluentNone(`${name}()`);
  }

  if (typeof func !== "function") {
    errors.push(new TypeError(`Function ${name}() is not callable`));
    return new FluentNone(`${name}()`);
  }

  return func;
}

/**
 * Resolve a call to a Function with positional and key-value arguments.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {Object} expr.fun
 *    FTL Function object.
 * @param   {Array} expr.args
 *    FTL Function argument list.
 * @returns {FluentType}
 * @private
 */
function CallExpression(env, { fun, args }) {
  const callee = FunctionReference(env, fun);

  if (callee instanceof FluentNone) {
    return callee;
  }

  const posargs = [];
  const keyargs = {};

  for (const arg of args) {
    if (arg.type === "narg") {
      keyargs[arg.name] = Type(env, arg.val);
    } else {
      posargs.push(Type(env, arg));
    }
  }

  try {
    return callee(posargs, keyargs);
  } catch (e) {
    // XXX Report errors.
    return new FluentNone();
  }
}

/**
 * Resolve a pattern (a complex string with placeables).
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Array} ptn
 *    Array of pattern elements.
 * @returns {Array}
 * @private
 */
function Pattern(env, ptn) {
  const { ctx, dirty, errors } = env;

  if (dirty.has(ptn)) {
    errors.push(new RangeError("Cyclic reference"));
    return new FluentNone();
  }

  // Tag the pattern as dirty for the purpose of the current resolution.
  dirty.add(ptn);
  const result = [];

  // Wrap interpolations with Directional Isolate Formatting characters
  // only when the pattern has more than one element.
  const useIsolating = ctx._useIsolating && ptn.length > 1;

  for (const elem of ptn) {
    if (typeof elem === "string") {
      result.push(elem);
      continue;
    }

    const part = Type(env, elem).toString(ctx);

    if (useIsolating) {
      result.push(FSI);
    }

    if (part.length > MAX_PLACEABLE_LENGTH) {
      errors.push(new RangeError("Too many characters in placeable " + `(${part.length}, max allowed is ${MAX_PLACEABLE_LENGTH})`));
      result.push(part.slice(MAX_PLACEABLE_LENGTH));
    } else {
      result.push(part);
    }

    if (useIsolating) {
      result.push(PDI);
    }
  }

  dirty.delete(ptn);
  return result.join("");
}

/**
 * Format a translation into a string.
 *
 * @param   {MessageContext} ctx
 *    A MessageContext instance which will be used to resolve the
 *    contextual information of the message.
 * @param   {Object}         args
 *    List of arguments provided by the developer which can be accessed
 *    from the message.
 * @param   {Object}         message
 *    An object with the Message to be resolved.
 * @param   {Array}          errors
 *    An error array that any encountered errors will be appended to.
 * @returns {FluentType}
 */
function resolve(ctx, args, message, errors = []) {
  const env = {
    ctx, args, errors, dirty: new WeakSet()
  };
  return Type(env, message).toString(ctx);
}
// CONCATENATED MODULE: ./node_modules/fluent/src/context.js



/**
 * Message contexts are single-language stores of translations.  They are
 * responsible for parsing translation resources in the Fluent syntax and can
 * format translation units (entities) to strings.
 *
 * Always use `MessageContext.format` to retrieve translation units from
 * a context.  Translations can contain references to other entities or
 * external arguments, conditional logic in form of select expressions, traits
 * which describe their grammatical features, and can use Fluent builtins which
 * make use of the `Intl` formatters to format numbers, dates, lists and more
 * into the context's language.  See the documentation of the Fluent syntax for
 * more information.
 */
class context_MessageContext {

  /**
   * Create an instance of `MessageContext`.
   *
   * The `locales` argument is used to instantiate `Intl` formatters used by
   * translations.  The `options` object can be used to configure the context.
   *
   * Examples:
   *
   *     const ctx = new MessageContext(locales);
   *
   *     const ctx = new MessageContext(locales, { useIsolating: false });
   *
   *     const ctx = new MessageContext(locales, {
   *       useIsolating: true,
   *       functions: {
   *         NODE_ENV: () => process.env.NODE_ENV
   *       }
   *     });
   *
   * Available options:
   *
   *   - `functions` - an object of additional functions available to
   *                   translations as builtins.
   *
   *   - `useIsolating` - boolean specifying whether to use Unicode isolation
   *                    marks (FSI, PDI) for bidi interpolations.
   *
   * @param   {string|Array<string>} locales - Locale or locales of the context
   * @param   {Object} [options]
   * @returns {MessageContext}
   */
  constructor(locales, { functions = {}, useIsolating = true } = {}) {
    this.locales = Array.isArray(locales) ? locales : [locales];

    this._terms = new Map();
    this._messages = new Map();
    this._functions = functions;
    this._useIsolating = useIsolating;
    this._intls = new WeakMap();
  }

  /*
   * Return an iterator over public `[id, message]` pairs.
   *
   * @returns {Iterator}
   */
  get messages() {
    return this._messages[Symbol.iterator]();
  }

  /*
   * Check if a message is present in the context.
   *
   * @param {string} id - The identifier of the message to check.
   * @returns {bool}
   */
  hasMessage(id) {
    return this._messages.has(id);
  }

  /*
   * Return the internal representation of a message.
   *
   * The internal representation should only be used as an argument to
   * `MessageContext.format`.
   *
   * @param {string} id - The identifier of the message to check.
   * @returns {Any}
   */
  getMessage(id) {
    return this._messages.get(id);
  }

  /**
   * Add a translation resource to the context.
   *
   * The translation resource must use the Fluent syntax.  It will be parsed by
   * the context and each translation unit (message) will be available in the
   * context by its identifier.
   *
   *     ctx.addMessages('foo = Foo');
   *     ctx.getMessage('foo');
   *
   *     // Returns a raw representation of the 'foo' message.
   *
   * Parsed entities should be formatted with the `format` method in case they
   * contain logic (references, select expressions etc.).
   *
   * @param   {string} source - Text resource with translations.
   * @returns {Array<Error>}
   */
  addMessages(source) {
    const [entries, errors] = parse(source);
    for (const id in entries) {
      if (id.startsWith("-")) {
        // Identifiers starting with a dash (-) define terms. Terms are private
        // and cannot be retrieved from MessageContext.
        if (this._terms.has(id)) {
          errors.push(`Attempt to override an existing term: "${id}"`);
          continue;
        }
        this._terms.set(id, entries[id]);
      } else {
        if (this._messages.has(id)) {
          errors.push(`Attempt to override an existing message: "${id}"`);
          continue;
        }
        this._messages.set(id, entries[id]);
      }
    }

    return errors;
  }

  /**
   * Format a message to a string or null.
   *
   * Format a raw `message` from the context into a string (or a null if it has
   * a null value).  `args` will be used to resolve references to external
   * arguments inside of the translation.
   *
   * In case of errors `format` will try to salvage as much of the translation
   * as possible and will still return a string.  For performance reasons, the
   * encountered errors are not returned but instead are appended to the
   * `errors` array passed as the third argument.
   *
   *     const errors = [];
   *     ctx.addMessages('hello = Hello, { $name }!');
   *     const hello = ctx.getMessage('hello');
   *     ctx.format(hello, { name: 'Jane' }, errors);
   *
   *     // Returns 'Hello, Jane!' and `errors` is empty.
   *
   *     ctx.format(hello, undefined, errors);
   *
   *     // Returns 'Hello, name!' and `errors` is now:
   *
   *     [<ReferenceError: Unknown external: name>]
   *
   * @param   {Object | string}    message
   * @param   {Object | undefined} args
   * @param   {Array}              errors
   * @returns {?string}
   */
  format(message, args, errors) {
    // optimize entities which are simple strings with no attributes
    if (typeof message === "string") {
      return message;
    }

    // optimize simple-string entities with attributes
    if (typeof message.val === "string") {
      return message.val;
    }

    // optimize entities with null values
    if (message.val === undefined) {
      return null;
    }

    return resolve(this, args, message, errors);
  }

  _memoizeIntlObject(ctor, opts) {
    const cache = this._intls.get(ctor) || {};
    const id = JSON.stringify(opts);

    if (!cache[id]) {
      cache[id] = new ctor(this.locales, opts);
      this._intls.set(ctor, cache);
    }

    return cache[id];
  }
}
// CONCATENATED MODULE: ./node_modules/fluent/src/cached_iterable.js
function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

/*
 * CachedIterable caches the elements yielded by an iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedIterable {
  /**
   * Create an `CachedIterable` instance.
   *
   * @param {Iterable} iterable
   * @returns {CachedIterable}
   */
  constructor(iterable) {
    if (Symbol.asyncIterator in Object(iterable)) {
      this.iterator = iterable[Symbol.asyncIterator]();
    } else if (Symbol.iterator in Object(iterable)) {
      this.iterator = iterable[Symbol.iterator]();
    } else {
      throw new TypeError("Argument must implement the iteration protocol.");
    }

    this.seen = [];
  }

  [Symbol.iterator]() {
    const { seen, iterator } = this;
    let cur = 0;

    return {
      next() {
        if (seen.length <= cur) {
          seen.push(iterator.next());
        }
        return seen[cur++];
      }
    };
  }

  [Symbol.asyncIterator]() {
    const { seen, iterator } = this;
    let cur = 0;

    return {
      next() {
        return _asyncToGenerator(function* () {
          if (seen.length <= cur) {
            seen.push((yield iterator.next()));
          }
          return seen[cur++];
        })();
      }
    };
  }

  /**
   * This method allows user to consume the next element from the iterator
   * into the cache.
   */
  touchNext() {
    const { seen, iterator } = this;
    if (seen.length === 0 || seen[seen.length - 1].done === false) {
      seen.push(iterator.next());
    }
  }
}
// CONCATENATED MODULE: ./node_modules/fluent/src/fallback.js
function _asyncIterator(iterable) { if (typeof Symbol === "function") { if (Symbol.asyncIterator) { var method = iterable[Symbol.asyncIterator]; if (method != null) return method.call(iterable); } if (Symbol.iterator) { return iterable[Symbol.iterator](); } } throw new TypeError("Object is not async iterable"); }

function fallback_asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

/*
 * @overview
 *
 * Functions for managing ordered sequences of MessageContexts.
 *
 * An ordered iterable of MessageContext instances can represent the current
 * negotiated fallback chain of languages.  This iterable can be used to find
 * the best existing translation for a given identifier.
 *
 * The mapContext* methods can be used to find the first MessageContext in the
 * given iterable which contains the translation with the given identifier.  If
 * the iterable is ordered according to the result of a language negotiation
 * the returned MessageContext contains the best available translation.
 *
 * A simple function which formats translations based on the identifier might
 * be implemented as follows:
 *
 *     formatString(id, args) {
 *         const ctx = mapContextSync(contexts, id);
 *
 *         if (ctx === null) {
 *             return id;
 *         }
 *
 *         const msg = ctx.getMessage(id);
 *         return ctx.format(msg, args);
 *     }
 *
 * In order to pass an iterator to mapContext*, wrap it in CachedIterable.
 * This allows multiple calls to mapContext* without advancing and eventually
 * depleting the iterator.
 *
 *     function *generateMessages() {
 *         // Some lazy logic for yielding MessageContexts.
 *         yield *[ctx1, ctx2];
 *     }
 *
 *     const contexts = new CachedIterable(generateMessages());
 *     const ctx = mapContextSync(contexts, id);
 *
 */

/*
 * Synchronously map an identifier or an array of identifiers to the best
 * `MessageContext` instance(s).
 *
 * @param {Iterable} iterable
 * @param {string|Array<string>} ids
 * @returns {MessageContext|Array<MessageContext>}
 */
function mapContextSync(iterable, ids) {
  if (!Array.isArray(ids)) {
    return getContextForId(iterable, ids);
  }

  return ids.map(id => getContextForId(iterable, id));
}

/*
 * Find the best `MessageContext` with the translation for `id`.
 */
function getContextForId(iterable, id) {
  for (const context of iterable) {
    if (context.hasMessage(id)) {
      return context;
    }
  }

  return null;
}

/*
 * Asynchronously map an identifier or an array of identifiers to the best
 * `MessageContext` instance(s).
 *
 * @param {AsyncIterable} iterable
 * @param {string|Array<string>} ids
 * @returns {Promise<MessageContext|Array<MessageContext>>}
 */
let mapContextAsync = (() => {
  var _ref = fallback_asyncToGenerator(function* (iterable, ids) {
    if (!Array.isArray(ids)) {
      var _iteratorNormalCompletion = true;
      var _didIteratorError = false;
      var _iteratorError = undefined;

      try {
        for (var _iterator = _asyncIterator(iterable), _step, _value; _step = yield _iterator.next(), _iteratorNormalCompletion = _step.done, _value = yield _step.value, !_iteratorNormalCompletion; _iteratorNormalCompletion = true) {
          const context = _value;

          if (context.hasMessage(ids)) {
            return context;
          }
        }
      } catch (err) {
        _didIteratorError = true;
        _iteratorError = err;
      } finally {
        try {
          if (!_iteratorNormalCompletion && _iterator.return) {
            yield _iterator.return();
          }
        } finally {
          if (_didIteratorError) {
            throw _iteratorError;
          }
        }
      }
    }

    let remainingCount = ids.length;
    const foundContexts = new Array(remainingCount).fill(null);

    var _iteratorNormalCompletion2 = true;
    var _didIteratorError2 = false;
    var _iteratorError2 = undefined;

    try {
      for (var _iterator2 = _asyncIterator(iterable), _step2, _value2; _step2 = yield _iterator2.next(), _iteratorNormalCompletion2 = _step2.done, _value2 = yield _step2.value, !_iteratorNormalCompletion2; _iteratorNormalCompletion2 = true) {
        const context = _value2;

        // XXX Switch to const [index, id] of id.entries() when we move to Babel 7.
        // See https://github.com/babel/babel/issues/5880.
        for (let index = 0; index < ids.length; index++) {
          const id = ids[index];
          if (!foundContexts[index] && context.hasMessage(id)) {
            foundContexts[index] = context;
            remainingCount--;
          }

          // Return early when all ids have been mapped to contexts.
          if (remainingCount === 0) {
            return foundContexts;
          }
        }
      }
    } catch (err) {
      _didIteratorError2 = true;
      _iteratorError2 = err;
    } finally {
      try {
        if (!_iteratorNormalCompletion2 && _iterator2.return) {
          yield _iterator2.return();
        }
      } finally {
        if (_didIteratorError2) {
          throw _iteratorError2;
        }
      }
    }

    return foundContexts;
  });

  return function mapContextAsync(_x, _x2) {
    return _ref.apply(this, arguments);
  };
})();
// CONCATENATED MODULE: ./node_modules/fluent/src/util.js
function nonBlank(line) {
  return !/^\s*$/.test(line);
}

function countIndent(line) {
  const [indent] = line.match(/^\s*/);
  return indent.length;
}

/**
 * Template literal tag for dedenting FTL code.
 *
 * Strip the common indent of non-blank lines. Remove blank lines.
 *
 * @param {Array<string>} strings
 */
function ftl(strings) {
  const [code] = strings;
  const lines = code.split("\n").filter(nonBlank);
  const indents = lines.map(countIndent);
  const common = Math.min(...indents);
  const indent = new RegExp(`^\\s{${common}}`);

  return lines.map(line => line.replace(indent, "")).join("\n");
}
// CONCATENATED MODULE: ./node_modules/fluent/src/index.js
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "_parse", function() { return parse; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "MessageContext", function() { return context_MessageContext; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "MessageArgument", function() { return FluentType; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "MessageNumberArgument", function() { return FluentNumber; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "MessageDateTimeArgument", function() { return FluentDateTime; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "CachedIterable", function() { return CachedIterable; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "mapContextSync", function() { return mapContextSync; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "mapContextAsync", function() { return mapContextAsync; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "ftl", function() { return ftl; });
/*
 * @module fluent
 * @overview
 *
 * `fluent` is a JavaScript implementation of Project Fluent, a localization
 * framework designed to unleash the expressive power of the natural language.
 *
 */











/***/ }),
/* 39 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(5);

// EXTERNAL MODULE: external "PropTypes"
var external_PropTypes_ = __webpack_require__(6);
var external_PropTypes_default = /*#__PURE__*/__webpack_require__.n(external_PropTypes_);

// EXTERNAL MODULE: ./node_modules/fluent/src/index.js + 8 modules
var src = __webpack_require__(38);

// CONCATENATED MODULE: ./node_modules/fluent-react/src/localization.js


/*
 * `ReactLocalization` handles translation formatting and fallback.
 *
 * The current negotiated fallback chain of languages is stored in the
 * `ReactLocalization` instance in form of an iterable of `MessageContext`
 * instances.  This iterable is used to find the best existing translation for
 * a given identifier.
 *
 * `Localized` components must subscribe to the changes of the
 * `ReactLocalization`'s fallback chain.  When the fallback chain changes (the
 * `messages` iterable is set anew), all subscribed compontent must relocalize.
 *
 * The `ReactLocalization` class instances are exposed to `Localized` elements
 * via the `LocalizationProvider` component.
 */
class localization_ReactLocalization {
  constructor(messages) {
    this.contexts = new src["CachedIterable"](messages);
    this.subs = new Set();
  }

  /*
   * Subscribe a `Localized` component to changes of `messages`.
   */
  subscribe(comp) {
    this.subs.add(comp);
  }

  /*
   * Unsubscribe a `Localized` component from `messages` changes.
   */
  unsubscribe(comp) {
    this.subs.delete(comp);
  }

  /*
   * Set a new `messages` iterable and trigger the retranslation.
   */
  setMessages(messages) {
    this.contexts = new src["CachedIterable"](messages);

    // Update all subscribed Localized components.
    this.subs.forEach(comp => comp.relocalize());
  }

  getMessageContext(id) {
    return Object(src["mapContextSync"])(this.contexts, id);
  }

  formatCompound(mcx, msg, args) {
    const value = mcx.format(msg, args);

    if (msg.attrs) {
      var attrs = {};
      for (const name of Object.keys(msg.attrs)) {
        attrs[name] = mcx.format(msg.attrs[name], args);
      }
    }

    return { value, attrs };
  }

  /*
   * Find a translation by `id` and format it to a string using `args`.
   */
  getString(id, args, fallback) {
    const mcx = this.getMessageContext(id);

    if (mcx === null) {
      return fallback || id;
    }

    const msg = mcx.getMessage(id);
    return mcx.format(msg, args);
  }
}

function isReactLocalization(props, propName) {
  const prop = props[propName];

  if (prop instanceof localization_ReactLocalization) {
    return null;
  }

  return new Error(`The ${propName} context field must be an instance of ReactLocalization.`);
}
// CONCATENATED MODULE: ./node_modules/fluent-react/src/provider.js





/*
 * The Provider component for the `ReactLocalization` class.
 *
 * Exposes a `ReactLocalization` instance to all descendants via React's
 * context feature.  It makes translations available to all localizable
 * elements in the descendant's render tree without the need to pass them
 * explicitly.
 *
 *     <LocalizationProvider messages={…}>
 *         …
 *     </LocalizationProvider>
 *
 * The `LocalizationProvider` component takes one prop: `messages`.  It should
 * be an iterable of `MessageContext` instances in order of the user's
 * preferred languages.  The `MessageContext` instances will be used by
 * `ReactLocalization` to format translations.  If a translation is missing in
 * one instance, `ReactLocalization` will fall back to the next one.
 */
class provider_LocalizationProvider extends external_React_["Component"] {
  constructor(props) {
    super(props);
    const { messages } = props;

    if (messages === undefined) {
      throw new Error("LocalizationProvider must receive the messages prop.");
    }

    if (!messages[Symbol.iterator]) {
      throw new Error("The messages prop must be an iterable.");
    }

    this.l10n = new localization_ReactLocalization(messages);
  }

  getChildContext() {
    return {
      l10n: this.l10n
    };
  }

  componentWillReceiveProps(next) {
    const { messages } = next;

    if (messages !== this.props.messages) {
      this.l10n.setMessages(messages);
    }
  }

  render() {
    return external_React_["Children"].only(this.props.children);
  }
}

provider_LocalizationProvider.childContextTypes = {
  l10n: isReactLocalization
};

provider_LocalizationProvider.propTypes = {
  children: external_PropTypes_default.a.element.isRequired,
  messages: isIterable
};

function isIterable(props, propName, componentName) {
  const prop = props[propName];

  if (Symbol.iterator in Object(prop)) {
    return null;
  }

  return new Error(`The ${propName} prop supplied to ${componentName} must be an iterable.`);
}
// CONCATENATED MODULE: ./node_modules/fluent-react/src/with_localization.js




function withLocalization(Inner) {
  class WithLocalization extends external_React_["Component"] {
    componentDidMount() {
      const { l10n } = this.context;

      if (l10n) {
        l10n.subscribe(this);
      }
    }

    componentWillUnmount() {
      const { l10n } = this.context;

      if (l10n) {
        l10n.unsubscribe(this);
      }
    }

    /*
     * Rerender this component in a new language.
     */
    relocalize() {
      // When the `ReactLocalization`'s fallback chain changes, update the
      // component.
      this.forceUpdate();
    }

    /*
     * Find a translation by `id` and format it to a string using `args`.
     */
    getString(id, args, fallback) {
      const { l10n } = this.context;

      if (!l10n) {
        return fallback || id;
      }

      return l10n.getString(id, args, fallback);
    }

    render() {
      return Object(external_React_["createElement"])(Inner, Object.assign(
      // getString needs to be re-bound on updates to trigger a re-render
      { getString: (...args) => this.getString(...args) }, this.props));
    }
  }

  WithLocalization.displayName = `WithLocalization(${displayName(Inner)})`;

  WithLocalization.contextTypes = {
    l10n: isReactLocalization
  };

  return WithLocalization;
}

function displayName(component) {
  return component.displayName || component.name || "Component";
}
// CONCATENATED MODULE: ./node_modules/fluent-react/src/markup.js
/* eslint-env browser */

const TEMPLATE = document.createElement("template");

function parseMarkup(str) {
  TEMPLATE.innerHTML = str;
  return TEMPLATE.content;
}
// CONCATENATED MODULE: ./node_modules/fluent-react/vendor/omittedCloseTags.js
/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in this directory.
 */

// For HTML, certain tags should omit their close tag. We keep a whitelist for
// those special-case tags.

var omittedCloseTags = {
  area: true,
  base: true,
  br: true,
  col: true,
  embed: true,
  hr: true,
  img: true,
  input: true,
  keygen: true,
  link: true,
  meta: true,
  param: true,
  source: true,
  track: true,
  wbr: true
  // NOTE: menuitem's close tag should be omitted, but that causes problems.
};

/* harmony default export */ var vendor_omittedCloseTags = (omittedCloseTags);
// CONCATENATED MODULE: ./node_modules/fluent-react/vendor/voidElementTags.js
/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in this directory.
 */



// For HTML, certain tags cannot have children. This has the same purpose as
// `omittedCloseTags` except that `menuitem` should still have its closing tag.

var voidElementTags = Object.assign({
  menuitem: true
}, vendor_omittedCloseTags);

/* harmony default export */ var vendor_voidElementTags = (voidElementTags);
// CONCATENATED MODULE: ./node_modules/fluent-react/src/localized.js







// Match the opening angle bracket (<) in HTML tags, and HTML entities like
// &amp;, &#0038;, &#x0026;.
const reMarkup = /<|&#?\w+;/;

/*
 * Prepare props passed to `Localized` for formatting.
 */
function toArguments(props) {
  const args = {};
  const elems = {};

  for (const [propname, propval] of Object.entries(props)) {
    if (propname.startsWith("$")) {
      const name = propname.substr(1);
      args[name] = propval;
    } else if (Object(external_React_["isValidElement"])(propval)) {
      // We'll try to match localNames of elements found in the translation with
      // names of elements passed as props. localNames are always lowercase.
      const name = propname.toLowerCase();
      elems[name] = propval;
    }
  }

  return [args, elems];
}

/*
 * The `Localized` class renders its child with translated props and children.
 *
 *     <Localized id="hello-world">
 *         <p>{'Hello, world!'}</p>
 *     </Localized>
 *
 * The `id` prop should be the unique identifier of the translation.  Any
 * attributes found in the translation will be applied to the wrapped element.
 *
 * Arguments to the translation can be passed as `$`-prefixed props on
 * `Localized`.
 *
 *     <Localized id="hello-world" $username={name}>
 *         <p>{'Hello, { $username }!'}</p>
 *     </Localized>
 *
 *  It's recommended that the contents of the wrapped component be a string
 *  expression.  The string will be used as the ultimate fallback if no
 *  translation is available.  It also makes it easy to grep for strings in the
 *  source code.
 */
class localized_Localized extends external_React_["Component"] {
  componentDidMount() {
    const { l10n } = this.context;

    if (l10n) {
      l10n.subscribe(this);
    }
  }

  componentWillUnmount() {
    const { l10n } = this.context;

    if (l10n) {
      l10n.unsubscribe(this);
    }
  }

  /*
   * Rerender this component in a new language.
   */
  relocalize() {
    // When the `ReactLocalization`'s fallback chain changes, update the
    // component.
    this.forceUpdate();
  }

  render() {
    const { l10n } = this.context;
    const { id, attrs, children } = this.props;
    const elem = external_React_["Children"].only(children);

    if (!l10n) {
      // Use the wrapped component as fallback.
      return elem;
    }

    const mcx = l10n.getMessageContext(id);

    if (mcx === null) {
      // Use the wrapped component as fallback.
      return elem;
    }

    const msg = mcx.getMessage(id);
    const [args, elems] = toArguments(this.props);
    const {
      value: messageValue,
      attrs: messageAttrs
    } = l10n.formatCompound(mcx, msg, args);

    // The default is to forbid all message attributes. If the attrs prop exists
    // on the Localized instance, only set message attributes which have been
    // explicitly allowed by the developer.
    if (attrs && messageAttrs) {
      var localizedProps = {};

      for (const [name, value] of Object.entries(messageAttrs)) {
        if (attrs[name]) {
          localizedProps[name] = value;
        }
      }
    }

    // If the wrapped component is a known void element, explicitly dismiss the
    // message value and do not pass it to cloneElement in order to avoid the
    // "void element tags must neither have `children` nor use
    // `dangerouslySetInnerHTML`" error.
    if (elem.type in vendor_voidElementTags) {
      return Object(external_React_["cloneElement"])(elem, localizedProps);
    }

    // If the message has a null value, we're only interested in its attributes.
    // Do not pass the null value to cloneElement as it would nuke all children
    // of the wrapped component.
    if (messageValue === null) {
      return Object(external_React_["cloneElement"])(elem, localizedProps);
    }

    // If the message value doesn't contain any markup nor any HTML entities,
    // insert it as the only child of the wrapped component.
    if (!reMarkup.test(messageValue)) {
      return Object(external_React_["cloneElement"])(elem, localizedProps, messageValue);
    }

    // If the message contains markup, parse it and try to match the children
    // found in the translation with the props passed to this Localized.
    const translationNodes = Array.from(parseMarkup(messageValue).childNodes);
    const translatedChildren = translationNodes.map(childNode => {
      if (childNode.nodeType === childNode.TEXT_NODE) {
        return childNode.textContent;
      }

      // If the child is not expected just take its textContent.
      if (!elems.hasOwnProperty(childNode.localName)) {
        return childNode.textContent;
      }

      const sourceChild = elems[childNode.localName];

      // If the element passed as a prop to <Localized> is a known void element,
      // explicitly dismiss any textContent which might have accidentally been
      // defined in the translation to prevent the "void element tags must not
      // have children" error.
      if (sourceChild.type in vendor_voidElementTags) {
        return sourceChild;
      }

      // TODO Protect contents of elements wrapped in <Localized>
      // https://github.com/projectfluent/fluent.js/issues/184
      // TODO  Control localizable attributes on elements passed as props
      // https://github.com/projectfluent/fluent.js/issues/185
      return Object(external_React_["cloneElement"])(sourceChild, null, childNode.textContent);
    });

    return Object(external_React_["cloneElement"])(elem, localizedProps, ...translatedChildren);
  }
}

localized_Localized.contextTypes = {
  l10n: isReactLocalization
};

localized_Localized.propTypes = {
  children: external_PropTypes_default.a.element.isRequired
};
// CONCATENATED MODULE: ./node_modules/fluent-react/src/index.js
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "LocalizationProvider", function() { return provider_LocalizationProvider; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "withLocalization", function() { return withLocalization; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "Localized", function() { return localized_Localized; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "ReactLocalization", function() { return localization_ReactLocalization; });
/* concated harmony reexport */__webpack_require__.d(__webpack_exports__, "isReactLocalization", function() { return isReactLocalization; });
/*
 * @module fluent-react
 * @overview
 *

 * `fluent-react` provides React bindings for Fluent.  It takes advantage of
 * React's Components system and the virtual DOM.  Translations are exposed to
 * components via the provider pattern.
 *
 *     <LocalizationProvider messages={…}>
 *         <Localized id="hello-world">
 *             <p>{'Hello, world!'}</p>
 *         </Localized>
 *     </LocalizationProvider>
 *
 * Consult the documentation of the `LocalizationProvider` and the `Localized`
 * components for more information.
 */






/***/ }),
/* 40 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(5);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// EXTERNAL MODULE: ./content-src/asrouter/template-utils.js
var template_utils = __webpack_require__(11);

// CONCATENATED MODULE: ./content-src/asrouter/components/Button/Button.jsx



const ALLOWED_STYLE_TAGS = ["color", "backgroundColor"];

const Button = props => {
  const style = {};

  // Add allowed style tags from props, e.g. props.color becomes style={color: props.color}
  for (const tag of ALLOWED_STYLE_TAGS) {
    if (typeof props[tag] !== "undefined") {
      style[tag] = props[tag];
    }
  }
  // remove border if bg is set to something custom
  if (style.backgroundColor) {
    style.border = "0";
  }

  return external_React_default.a.createElement(
    "a",
    { href: Object(template_utils["safeURI"])(props.url),
      onClick: props.onClick,
      className: props.className || "ASRouterButton",
      style: style },
    props.children
  );
};
// CONCATENATED MODULE: ./content-src/asrouter/components/SnippetBase/SnippetBase.jsx


class SnippetBase_SnippetBase extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
  }

  onBlockClicked() {
    this.props.sendUserActionTelemetry({ event: "BLOCK", id: this.props.UISurface });
    this.props.onBlock();
  }

  render() {
    const { props } = this;

    const containerClassName = `SnippetBaseContainer${props.className ? ` ${props.className}` : ""}`;

    return external_React_default.a.createElement(
      "div",
      { className: containerClassName },
      external_React_default.a.createElement(
        "div",
        { className: "innerWrapper" },
        props.children
      ),
      external_React_default.a.createElement("button", { className: "blockButton", onClick: this.onBlockClicked })
    );
  }
}
// CONCATENATED MODULE: ./content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SimpleSnippet", function() { return SimpleSnippet_SimpleSnippet; });
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };






const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png";

class SimpleSnippet_SimpleSnippet extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  onButtonClick() {
    this.props.sendUserActionTelemetry({ event: "CLICK_BUTTON", id: this.props.UISurface });
  }

  renderTitle() {
    const { title } = this.props.content;
    return title ? external_React_default.a.createElement(
      "h3",
      { className: "title" },
      title
    ) : null;
  }

  renderTitleIcon() {
    const titleIcon = Object(template_utils["safeURI"])(this.props.content.title_icon);
    return titleIcon ? external_React_default.a.createElement("span", { className: "titleIcon", style: { backgroundImage: `url("${titleIcon}")` } }) : null;
  }

  renderButton(className) {
    const { props } = this;
    return external_React_default.a.createElement(
      Button,
      {
        className: className,
        onClick: this.onButtonClick,
        url: props.content.button_url,
        color: props.content.button_color,
        backgroundColor: props.content.button_background_color },
      props.content.button_label
    );
  }

  render() {
    const { props } = this;
    const hasLink = props.content.button_url && props.content.button_type === "anchor";
    const hasButton = props.content.button_url && !props.content.button_type;
    const className = `SimpleSnippet${props.content.tall ? " tall" : ""}`;
    return external_React_default.a.createElement(
      SnippetBase_SnippetBase,
      _extends({}, props, { className: className }),
      external_React_default.a.createElement("img", { src: Object(template_utils["safeURI"])(props.content.icon) || DEFAULT_ICON_PATH, className: "icon" }),
      external_React_default.a.createElement(
        "div",
        null,
        this.renderTitleIcon(),
        " ",
        this.renderTitle(),
        " ",
        external_React_default.a.createElement(
          "p",
          { className: "body" },
          props.richText || props.content.text
        ),
        " ",
        hasLink ? this.renderButton("ASRouterAnchor") : null
      ),
      hasButton ? external_React_default.a.createElement(
        "div",
        null,
        this.renderButton()
      ) : null
    );
  }
}

/***/ }),
/* 41 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(2);

// CONCATENATED MODULE: ./common/Dedupe.jsm
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
// CONCATENATED MODULE: ./common/Reducers.jsm
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_DEFAULT_ROWS", function() { return TOP_SITES_DEFAULT_ROWS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_MAX_SITES_PER_ROW", function() { return TOP_SITES_MAX_SITES_PER_ROW; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "INITIAL_STATE", function() { return INITIAL_STATE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "insertPinned", function() { return insertPinned; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "reducers", function() { return reducers; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */




const TOP_SITES_DEFAULT_ROWS = 1;
const TOP_SITES_MAX_SITES_PER_ROW = 8;


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
  Dialog: {
    visible: false,
    data: {}
  },
  Sections: []
};


function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case Actions["actionTypes"].INIT:
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
    case Actions["actionTypes"].TOP_SITES_UPDATED:
      if (!action.data || !action.data.links) {
        return prevState;
      }
      return Object.assign({}, prevState, { initialized: true, rows: action.data.links }, action.data.pref ? { pref: action.data.pref } : {});
    case Actions["actionTypes"].TOP_SITES_PREFS_UPDATED:
      return Object.assign({}, prevState, { pref: action.data.pref });
    case Actions["actionTypes"].TOP_SITES_EDIT:
      return Object.assign({}, prevState, {
        editForm: {
          index: action.data.index,
          previewResponse: null
        }
      });
    case Actions["actionTypes"].TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, { editForm: null });
    case Actions["actionTypes"].PREVIEW_RESPONSE:
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
    case Actions["actionTypes"].PREVIEW_REQUEST:
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
    case Actions["actionTypes"].PREVIEW_REQUEST_CANCEL:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null
        }
      });
    case Actions["actionTypes"].SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, { rows: newRows }) : prevState;
    case Actions["actionTypes"].PLACES_BOOKMARK_ADDED:
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
    case Actions["actionTypes"].PLACES_BOOKMARK_REMOVED:
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
    case Actions["actionTypes"].PLACES_LINK_DELETED:
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
    case Actions["actionTypes"].DIALOG_OPEN:
      return Object.assign({}, prevState, { visible: true, data: action.data });
    case Actions["actionTypes"].DIALOG_CANCEL:
      return Object.assign({}, prevState, { visible: false });
    case Actions["actionTypes"].DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case Actions["actionTypes"].PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, { initialized: true, values: action.data });
    case Actions["actionTypes"].PREF_CHANGED:
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
    case Actions["actionTypes"].SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case Actions["actionTypes"].SECTION_REGISTER:
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
    case Actions["actionTypes"].SECTION_UPDATE:
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
    case Actions["actionTypes"].SECTION_UPDATE_CARD:
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
    case Actions["actionTypes"].PLACES_BOOKMARK_ADDED:
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
    case Actions["actionTypes"].PLACES_SAVED_TO_POCKET:
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
    case Actions["actionTypes"].PLACES_BOOKMARK_REMOVED:
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
    case Actions["actionTypes"].PLACES_LINK_DELETED:
    case Actions["actionTypes"].PLACES_LINK_BLOCKED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.url !== action.data.url) }));
    case Actions["actionTypes"].DELETE_FROM_POCKET:
    case Actions["actionTypes"].ARCHIVE_FROM_POCKET:
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.pocket_id !== action.data.pocket_id) }));
    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case Actions["actionTypes"].SNIPPETS_DATA:
      return Object.assign({}, prevState, { initialized: true }, action.data);
    case Actions["actionTypes"].SNIPPET_BLOCKED:
      return Object.assign({}, prevState, { blockList: prevState.blockList.concat(action.data) });
    case Actions["actionTypes"].SNIPPETS_BLOCKLIST_CLEARED:
      return Object.assign({}, prevState, { blockList: [] });
    case Actions["actionTypes"].SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

var reducers = { TopSites, App, Snippets, Prefs, Dialog, Sections };

/***/ }),
/* 42 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(5);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// CONCATENATED MODULE: ./content-src/asrouter/components/ModalOverlay/ModalOverlay.jsx


class ModalOverlay_ModalOverlay extends external_React_default.a.PureComponent {
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
    return external_React_default.a.createElement(
      "div",
      null,
      external_React_default.a.createElement("div", { className: `modalOverlayOuter ${active ? "active" : ""}` }),
      external_React_default.a.createElement(
        "div",
        { className: `modalOverlayInner ${active ? "active" : ""}` },
        external_React_default.a.createElement(
          "h2",
          null,
          " ",
          title,
          " "
        ),
        this.props.children,
        external_React_default.a.createElement(
          "div",
          { className: "footer" },
          external_React_default.a.createElement(
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
// CONCATENATED MODULE: ./content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "OnboardingMessage", function() { return OnboardingMessage_OnboardingMessage; });
var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };




class OnboardingMessage_OnboardingCard extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    const { props } = this;
    props.sendUserActionTelemetry({ event: "CLICK_BUTTON", message_id: props.id, id: props.UISurface });
    props.onAction(props.content);
  }

  render() {
    const { content } = this.props;
    return external_React_default.a.createElement(
      "div",
      { className: "onboardingMessage" },
      external_React_default.a.createElement("div", { className: `onboardingMessageImage ${content.icon}` }),
      external_React_default.a.createElement(
        "div",
        { className: "onboardingContent" },
        external_React_default.a.createElement(
          "span",
          null,
          external_React_default.a.createElement(
            "h3",
            null,
            " ",
            content.title,
            " "
          ),
          external_React_default.a.createElement(
            "p",
            null,
            " ",
            content.text,
            " "
          )
        ),
        external_React_default.a.createElement(
          "span",
          null,
          external_React_default.a.createElement(
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

class OnboardingMessage_OnboardingMessage extends external_React_default.a.PureComponent {
  render() {
    const { props } = this;
    return external_React_default.a.createElement(
      ModalOverlay_ModalOverlay,
      _extends({}, props, { button_label: "Start Browsing", title: "Welcome to Firefox" }),
      external_React_default.a.createElement(
        "div",
        { className: "onboardingMessageContainer" },
        props.bundle.map(message => external_React_default.a.createElement(OnboardingMessage_OnboardingCard, _extends({ key: message.id,
          sendUserActionTelemetry: props.sendUserActionTelemetry,
          onAction: props.onAction,
          UISurface: props.UISurface
        }, message)))
      )
    );
  }
}

/***/ }),
/* 43 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(2);

// CONCATENATED MODULE: ./content-src/components/Card/types.js
const cardContextTypes = {
  history: {
    intlID: "type_label_visited",
    icon: "history-item"
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
var external_ReactRedux_ = __webpack_require__(16);

// EXTERNAL MODULE: external "ReactIntl"
var external_ReactIntl_ = __webpack_require__(13);

// EXTERNAL MODULE: ./content-src/lib/link-menu-options.js
var link_menu_options = __webpack_require__(23);

// EXTERNAL MODULE: ./content-src/components/LinkMenu/LinkMenu.jsx
var LinkMenu = __webpack_require__(24);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(5);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// EXTERNAL MODULE: ./content-src/lib/screenshot-utils.js
var screenshot_utils = __webpack_require__(26);

// CONCATENATED MODULE: ./content-src/components/Card/Card.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Card", function() { return Card_Card; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Card", function() { return Card; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PlaceholderCard", function() { return PlaceholderCard; });
function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }










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
class Card_Card extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      activeCard: null,
      imageLoaded: false,
      showContextMenu: false,
      cardImage: null
    };
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  /**
   * Helper to conditionally load an image and update state when it loads.
   */
  maybeLoadImage() {
    var _this = this;

    return _asyncToGenerator(function* () {
      // No need to load if it's already loaded or no image
      const { cardImage } = _this.state;
      if (!cardImage) {
        return;
      }

      const imageUrl = cardImage.url;
      if (!_this.state.imageLoaded) {
        // Initialize a promise to share a load across multiple card updates
        if (!gImageLoading.has(imageUrl)) {
          const loaderPromise = new Promise(function (resolve, reject) {
            const loader = new Image();
            loader.addEventListener("load", resolve);
            loader.addEventListener("error", reject);
            loader.src = imageUrl;
          });

          // Save and remove the promise only while it's pending
          gImageLoading.set(imageUrl, loaderPromise);
          loaderPromise.catch(function (ex) {
            return ex;
          }).then(function () {
            return gImageLoading.delete(imageUrl);
          }).catch();
        }

        // Wait for the image whether just started loading or reused promise
        yield gImageLoading.get(imageUrl);

        // Only update state if we're still waiting to load the original image
        if (screenshot_utils["ScreenshotUtils"].isRemoteImageLocal(_this.state.cardImage, _this.props.link.image) && !_this.state.imageLoaded) {
          _this.setState({ imageLoaded: true });
        }
      }
    })();
  }

  /**
   * Helper to obtain the next state based on nextProps and prevState.
   *
   * NOTE: Rename this method to getDerivedStateFromProps when we update React
   *       to >= 16.3. We will need to update tests as well. We cannot rename this
   *       method to getDerivedStateFromProps now because there is a mismatch in
   *       the React version that we are using for both testing and production.
   *       (i.e. react-test-render => "16.3.2", react => "16.2.0").
   *
   * See https://github.com/airbnb/enzyme/blob/master/packages/enzyme-adapter-react-16/package.json#L43.
   */
  static getNextStateFromProps(nextProps, prevState) {
    const { image } = nextProps.link;
    const imageInState = screenshot_utils["ScreenshotUtils"].isRemoteImageLocal(prevState.cardImage, image);
    let nextState = null;

    // Image is updating.
    if (!imageInState && nextProps.link) {
      nextState = { imageLoaded: false };
    }

    if (imageInState) {
      return nextState;
    }

    // Since image was updated, attempt to revoke old image blob URL, if it exists.
    screenshot_utils["ScreenshotUtils"].maybeRevokeBlobObjectURL(prevState.cardImage);

    nextState = nextState || {};
    nextState.cardImage = screenshot_utils["ScreenshotUtils"].createLocalImageObject(image);

    return nextState;
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
      this.props.dispatch(Actions["actionCreators"].OnlyToMain({
        type: Actions["actionTypes"].SHOW_DOWNLOAD_FILE,
        data: this.props.link
      }));
    } else {
      const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
      this.props.dispatch(Actions["actionCreators"].OnlyToMain({
        type: Actions["actionTypes"].OPEN_LINK,
        data: Object.assign(this.props.link, { event: { altKey, button, ctrlKey, metaKey, shiftKey } })
      }));
    }
    if (this.props.isWebExtension) {
      this.props.dispatch(Actions["actionCreators"].WebExtEvent(Actions["actionTypes"].WEBEXT_CLICK, {
        source: this.props.eventSource,
        url: this.props.link.url,
        action_position: this.props.index
      }));
    } else {
      this.props.dispatch(Actions["actionCreators"].UserEvent(Object.assign({
        event: "CLICK",
        source: this.props.eventSource,
        action_position: this.props.index
      }, this._getTelemetryInfo())));

      if (this.props.shouldSendImpressionStats) {
        this.props.dispatch(Actions["actionCreators"].ImpressionStats({
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

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillMount() {
    const nextState = Card_Card.getNextStateFromProps(this.props, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillReceiveProps(nextProps) {
    const nextState = Card_Card.getNextStateFromProps(nextProps, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  componentWillUnmount() {
    screenshot_utils["ScreenshotUtils"].maybeRevokeBlobObjectURL(this.state.cardImage);
  }

  render() {
    const { index, className, link, dispatch, contextMenuOptions, eventSource, shouldSendImpressionStats } = this.props;
    const { props } = this;
    const isContextMenuOpen = this.state.showContextMenu && this.state.activeCard === index;
    // Display "now" as "trending" until we have new strings #3402
    const { icon, intlID } = cardContextTypes[link.type === "now" ? "trending" : link.type] || {};
    const hasImage = this.state.cardImage || link.hasImage;
    const imageStyle = { backgroundImage: this.state.cardImage ? `url(${this.state.cardImage.url})` : "none" };
    const outerClassName = ["card-outer", className, isContextMenuOpen && "active", props.placeholder && "placeholder"].filter(v => v).join(" ");

    return external_React_default.a.createElement(
      "li",
      { className: outerClassName },
      external_React_default.a.createElement(
        "a",
        { href: link.type === "pocket" ? link.open_url : link.url, onClick: !props.placeholder ? this.onLinkClick : undefined },
        external_React_default.a.createElement(
          "div",
          { className: "card" },
          external_React_default.a.createElement(
            "div",
            { className: "card-preview-image-outer" },
            hasImage && external_React_default.a.createElement("div", { className: `card-preview-image${this.state.imageLoaded ? " loaded" : ""}`, style: imageStyle })
          ),
          external_React_default.a.createElement(
            "div",
            { className: "card-details" },
            link.type === "download" && external_React_default.a.createElement(
              "div",
              { className: "card-host-name alternate" },
              external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: Object(link_menu_options["GetPlatformString"])(this.props.platform) })
            ),
            link.hostname && external_React_default.a.createElement(
              "div",
              { className: "card-host-name" },
              link.hostname.slice(0, 100),
              link.type === "download" && `  \u2014 ${link.description}`
            ),
            external_React_default.a.createElement(
              "div",
              { className: ["card-text", icon ? "" : "no-context", link.description ? "" : "no-description", link.hostname ? "" : "no-host-name"].join(" ") },
              external_React_default.a.createElement(
                "h4",
                { className: "card-title", dir: "auto" },
                link.title
              ),
              external_React_default.a.createElement(
                "p",
                { className: "card-description", dir: "auto" },
                link.description
              )
            ),
            external_React_default.a.createElement(
              "div",
              { className: "card-context" },
              icon && !link.context && external_React_default.a.createElement("span", { className: `card-context-icon icon icon-${icon}` }),
              link.icon && link.context && external_React_default.a.createElement("span", { className: "card-context-icon icon", style: { backgroundImage: `url('${link.icon}')` } }),
              intlID && !link.context && external_React_default.a.createElement(
                "div",
                { className: "card-context-label" },
                external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: intlID, defaultMessage: "Visited" })
              ),
              link.context && external_React_default.a.createElement(
                "div",
                { className: "card-context-label" },
                link.context
              )
            )
          )
        )
      ),
      !props.placeholder && external_React_default.a.createElement(
        "button",
        { className: "context-menu-button icon",
          onClick: this.onMenuButtonClick },
        external_React_default.a.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${link.title}`
        )
      ),
      isContextMenuOpen && external_React_default.a.createElement(LinkMenu["LinkMenu"], {
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
Card_Card.defaultProps = { link: {} };
const Card = Object(external_ReactRedux_["connect"])(state => ({ platform: state.Prefs.values.platform }))(Card_Card);
const PlaceholderCard = props => external_React_default.a.createElement(Card, { placeholder: true, className: props.className });

/***/ }),
/* 44 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";

// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(2);

// EXTERNAL MODULE: external "ReactIntl"
var external_ReactIntl_ = __webpack_require__(13);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(5);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// EXTERNAL MODULE: ./content-src/components/TopSites/TopSitesConstants.js
var TopSitesConstants = __webpack_require__(34);

// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteFormInput.jsx



class TopSiteFormInput_TopSiteFormInput extends external_React_default.a.PureComponent {
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

    return external_React_default.a.createElement(
      "label",
      null,
      external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: this.props.titleId }),
      external_React_default.a.createElement(
        "div",
        { className: `field ${typeUrl ? "url" : ""}${validationError ? " invalid" : ""}` },
        this.props.loading ? external_React_default.a.createElement(
          "div",
          { className: "loading-container" },
          external_React_default.a.createElement("div", { className: "loading-animation" })
        ) : showClearButton && external_React_default.a.createElement("div", { className: "icon icon-clear-input", onClick: this.props.onClear }),
        external_React_default.a.createElement("input", { type: "text",
          value: this.props.value,
          ref: this.onMount,
          onChange: this.onChange,
          placeholder: this.props.intl.formatMessage({ id: this.props.placeholderId }),
          autoFocus: this.props.shouldFocus,
          disabled: this.props.loading }),
        validationError && external_React_default.a.createElement(
          "aside",
          { className: "error-tooltip" },
          external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: this.props.errorMessageId })
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
// EXTERNAL MODULE: ./content-src/components/TopSites/TopSite.jsx
var TopSite = __webpack_require__(35);

// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteForm.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteForm", function() { return TopSiteForm_TopSiteForm; });







class TopSiteForm_TopSiteForm extends external_React_default.a.PureComponent {
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
    this.props.dispatch({ type: Actions["actionTypes"].PREVIEW_REQUEST_CANCEL });
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
      this.props.dispatch(Actions["actionCreators"].AlsoToMain({
        type: Actions["actionTypes"].TOP_SITES_PIN,
        data: { site, index }
      }));
      this.props.dispatch(Actions["actionCreators"].UserEvent({
        source: TopSitesConstants["TOP_SITES_SOURCE"],
        event: "TOP_SITES_EDIT",
        action_position: index
      }));

      this.props.onClose();
    }
  }

  onPreviewButtonClick(event) {
    event.preventDefault();
    if (this.validateForm()) {
      this.props.dispatch(Actions["actionCreators"].AlsoToMain({
        type: Actions["actionTypes"].PREVIEW_REQUEST,
        data: { url: this.cleanUrl(this.state.customScreenshotUrl) }
      }));
      this.props.dispatch(Actions["actionCreators"].UserEvent({
        source: TopSitesConstants["TOP_SITES_SOURCE"],
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
      return external_React_default.a.createElement(
        "a",
        { className: "enable-custom-image-input", onClick: this.onEnableScreenshotUrlForm },
        external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: "topsites_form_use_image_link" })
      );
    }
    return external_React_default.a.createElement(
      "div",
      { className: "custom-image-input-container" },
      external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, {
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
    return external_React_default.a.createElement(
      "form",
      { className: "topsite-form" },
      external_React_default.a.createElement(
        "div",
        { className: "form-input-container" },
        external_React_default.a.createElement(
          "h3",
          { className: "section-title" },
          external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: showAsAdd ? "topsites_form_add_header" : "topsites_form_edit_header" })
        ),
        external_React_default.a.createElement(
          "div",
          { className: "fields-and-preview" },
          external_React_default.a.createElement(
            "div",
            { className: "form-wrapper" },
            external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, { onChange: this.onLabelChange,
              value: this.state.label,
              titleId: "topsites_form_title_label",
              placeholderId: "topsites_form_title_placeholder",
              intl: this.props.intl }),
            external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, { onChange: this.onUrlChange,
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
          external_React_default.a.createElement(TopSite["TopSiteLink"], { link: previewLink,
            defaultStyle: requestFailed,
            title: this.state.label })
        )
      ),
      external_React_default.a.createElement(
        "section",
        { className: "actions" },
        external_React_default.a.createElement(
          "button",
          { className: "cancel", type: "button", onClick: this.onCancelButtonClick },
          external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: "topsites_form_cancel_button" })
        ),
        previewMode ? external_React_default.a.createElement(
          "button",
          { className: "done preview", type: "submit", onClick: this.onPreviewButtonClick },
          external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: "topsites_form_preview_button" })
        ) : external_React_default.a.createElement(
          "button",
          { className: "done", type: "submit", onClick: this.onDoneButtonClick },
          external_React_default.a.createElement(external_ReactIntl_["FormattedMessage"], { id: showAsAdd ? "topsites_form_add_button" : "topsites_form_save_button" })
        )
      )
    );
  }
}

TopSiteForm_TopSiteForm.defaultProps = {
  site: null,
  index: -1
};

/***/ })
/******/ ]);
//# sourceMappingURL=activity-stream.bundle.js.map