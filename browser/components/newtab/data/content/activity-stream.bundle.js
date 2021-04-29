/*! THIS FILE IS AUTO-GENERATED: webpack.system-addon.config.js */
var NewtabRenderUtils =
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
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "NewTab", function() { return NewTab; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "renderWithoutState", function() { return renderWithoutState; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "renderCache", function() { return renderCache; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_Base_Base__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);
/* harmony import */ var content_src_lib_detect_user_session_start__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(66);
/* harmony import */ var content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(67);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(14);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react_dom__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(73);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */








const NewTab = ({
  store
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(react_redux__WEBPACK_IMPORTED_MODULE_4__["Provider"], {
  store: store
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(content_src_components_Base_Base__WEBPACK_IMPORTED_MODULE_1__["Base"], null));
function renderWithoutState() {
  const store = Object(content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_3__["initStore"])(common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_7__["reducers"]);
  new content_src_lib_detect_user_session_start__WEBPACK_IMPORTED_MODULE_2__["DetectUserSessionStart"](store).sendEventOrAddListener(); // If this document has already gone into the background by the time we've reached
  // here, we can deprioritize requesting the initial state until the event loop
  // frees up. If, however, the visibility changes, we then send the request.

  let didRequest = false;
  let requestIdleCallbackId = 0;

  function doRequest() {
    if (!didRequest) {
      if (requestIdleCallbackId) {
        cancelIdleCallback(requestIdleCallbackId);
      }

      didRequest = true;
      store.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST
      }));
    }
  }

  if (document.hidden) {
    requestIdleCallbackId = requestIdleCallback(doRequest);
    addEventListener("visibilitychange", doRequest, {
      once: true
    });
  } else {
    doRequest();
  }

  react_dom__WEBPACK_IMPORTED_MODULE_6___default.a.hydrate( /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(NewTab, {
    store: store
  }), document.getElementById("root"));
}
function renderCache(initialState) {
  const store = Object(content_src_lib_init_store__WEBPACK_IMPORTED_MODULE_3__["initStore"])(common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_7__["reducers"], initialState);
  new content_src_lib_detect_user_session_start__WEBPACK_IMPORTED_MODULE_2__["DetectUserSessionStart"](store).sendEventOrAddListener();
  react_dom__WEBPACK_IMPORTED_MODULE_6___default.a.hydrate( /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(NewTab, {
    store: store
  }), document.getElementById("root"));
}

/***/ }),
/* 1 */
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

const globalImportContext = typeof Window === "undefined" ? BACKGROUND_PROCESS : UI_CODE; // Export for tests

// Create an object that avoids accidental differing key/value pairs:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
const actionTypes = {};

for (const type of ["ABOUT_SPONSORED_TOP_SITES", "ADDONS_INFO_REQUEST", "ADDONS_INFO_RESPONSE", "ARCHIVE_FROM_POCKET", "AS_ROUTER_INITIALIZED", "AS_ROUTER_PREF_CHANGED", "AS_ROUTER_TARGETING_UPDATE", "AS_ROUTER_TELEMETRY_USER_EVENT", "BLOCK_URL", "BOOKMARK_URL", "CLEAR_PREF", "COPY_DOWNLOAD_LINK", "DELETE_BOOKMARK_BY_ID", "DELETE_FROM_POCKET", "DELETE_HISTORY_URL", "DIALOG_CANCEL", "DIALOG_OPEN", "DISABLE_SEARCH", "DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE", "DISCOVERY_STREAM_CONFIG_CHANGE", "DISCOVERY_STREAM_CONFIG_RESET", "DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS", "DISCOVERY_STREAM_CONFIG_SETUP", "DISCOVERY_STREAM_CONFIG_SET_VALUE", "DISCOVERY_STREAM_DEV_EXPIRE_CACHE", "DISCOVERY_STREAM_DEV_IDLE_DAILY", "DISCOVERY_STREAM_DEV_SYNC_RS", "DISCOVERY_STREAM_DEV_SYSTEM_TICK", "DISCOVERY_STREAM_FEEDS_UPDATE", "DISCOVERY_STREAM_FEED_UPDATE", "DISCOVERY_STREAM_IMPRESSION_STATS", "DISCOVERY_STREAM_LAYOUT_RESET", "DISCOVERY_STREAM_LAYOUT_UPDATE", "DISCOVERY_STREAM_LINK_BLOCKED", "DISCOVERY_STREAM_LOADED_CONTENT", "DISCOVERY_STREAM_PERSONALIZATION_INIT", "DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED", "DISCOVERY_STREAM_PERSONALIZATION_VERSION", "DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE", "DISCOVERY_STREAM_RETRY_FEED", "DISCOVERY_STREAM_SPOCS_CAPS", "DISCOVERY_STREAM_SPOCS_ENDPOINT", "DISCOVERY_STREAM_SPOCS_PLACEMENTS", "DISCOVERY_STREAM_SPOCS_UPDATE", "DISCOVERY_STREAM_SPOC_BLOCKED", "DISCOVERY_STREAM_SPOC_IMPRESSION", "DOWNLOAD_CHANGED", "FAKE_FOCUS_SEARCH", "FILL_SEARCH_TERM", "HANDOFF_SEARCH_TO_AWESOMEBAR", "HIDE_PRIVACY_INFO", "INIT", "NEW_TAB_INIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_REHYDRATED", "NEW_TAB_STATE_REQUEST", "NEW_TAB_UNLOAD", "OPEN_DOWNLOAD_FILE", "OPEN_LINK", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "OPEN_WEBEXT_SETTINGS", "PARTNER_LINK_ATTRIBUTION", "PLACES_BOOKMARKS_REMOVED", "PLACES_BOOKMARK_ADDED", "PLACES_HISTORY_CLEARED", "PLACES_LINKS_CHANGED", "PLACES_LINKS_DELETED", "PLACES_LINK_BLOCKED", "PLACES_SAVED_TO_POCKET", "POCKET_CTA", "POCKET_LINK_DELETED_OR_ARCHIVED", "POCKET_LOGGED_IN", "POCKET_WAITING_FOR_SPOC", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "PREVIEW_REQUEST", "PREVIEW_REQUEST_CANCEL", "PREVIEW_RESPONSE", "REMOVE_DOWNLOAD_FILE", "RICH_ICON_MISSING", "SAVE_SESSION_PERF_DATA", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SECTION_DEREGISTER", "SECTION_DISABLE", "SECTION_ENABLE", "SECTION_MOVE", "SECTION_OPTIONS_CHANGED", "SECTION_REGISTER", "SECTION_UPDATE", "SECTION_UPDATE_CARD", "SETTINGS_CLOSE", "SETTINGS_OPEN", "SET_PREF", "SHOW_DOWNLOAD_FILE", "SHOW_FIREFOX_ACCOUNTS", "SHOW_PRIVACY_INFO", "SHOW_SEARCH", "SKIPPED_SIGNIN", "SNIPPETS_BLOCKLIST_CLEARED", "SNIPPETS_BLOCKLIST_UPDATED", "SNIPPETS_DATA", "SNIPPETS_PREVIEW_MODE", "SNIPPETS_RESET", "SNIPPET_BLOCKED", "SUBMIT_EMAIL", "SUBMIT_SIGNIN", "SYSTEM_TICK", "TELEMETRY_IMPRESSION_STATS", "TELEMETRY_USER_EVENT", "TOP_SITES_CANCEL_EDIT", "TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL", "TOP_SITES_EDIT", "TOP_SITES_IMPRESSION_STATS", "TOP_SITES_INSERT", "TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL", "TOP_SITES_PIN", "TOP_SITES_PREFS_UPDATED", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "TOTAL_BOOKMARKS_REQUEST", "TOTAL_BOOKMARKS_RESPONSE", "UNINIT", "UPDATE_PINNED_SEARCH_SHORTCUTS", "UPDATE_SEARCH_SHORTCUTS", "UPDATE_SECTION_PREFS", "WEBEXT_CLICK", "WEBEXT_DISMISS"]) {
  actionTypes[type] = type;
} // Helper function for creating routed actions between content and main
// Not intended to be used by consumers


function _RouteMessage(action, options) {
  const meta = action.meta ? { ...action.meta
  } : {};

  if (!options || !options.from || !options.to) {
    throw new Error("Routed Messages must have options as the second parameter, and must at least include a .from and .to property.");
  } // For each of these fields, if they are passed as an option,
  // add them to the action. If they are not defined, remove them.


  ["from", "to", "toTarget", "fromTarget", "skipMain", "skipLocal"].forEach(o => {
    if (typeof options[o] !== "undefined") {
      meta[o] = options[o];
    } else if (meta[o]) {
      delete meta[o];
    }
  });
  return { ...action,
    meta
  };
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
/**
 * DiscoveryStreamImpressionStats - A telemetry ping indicating an impression stats in Discovery Stream.
 *
 * @param  {object} data Fields to include in the ping
 * @param  {int} importContext (For testing) Override the import context for testing.
 * #return {object} An action. For UI code, a AlsoToMain action.
 */


function DiscoveryStreamImpressionStats(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.DISCOVERY_STREAM_IMPRESSION_STATS,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}
/**
 * DiscoveryStreamLoadedContent - A telemetry ping indicating a content gets loaded in Discovery Stream.
 *
 * @param  {object} data Fields to include in the ping
 * @param  {int} importContext (For testing) Override the import context for testing.
 * #return {object} An action. For UI code, a AlsoToMain action.
 */


function DiscoveryStreamLoadedContent(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.DISCOVERY_STREAM_LOADED_CONTENT,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function SetPref(name, value, importContext = globalImportContext) {
  const action = {
    type: actionTypes.SET_PREF,
    data: {
      name,
      value
    }
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function WebExtEvent(type, data, importContext = globalImportContext) {
  if (!data || !data.source) {
    throw new Error('WebExtEvent actions should include a property "source", the id of the webextension that should receive the event.');
  }

  const action = {
    type,
    data
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

var actionCreators = {
  BroadcastToContent,
  UserEvent,
  ASRouterUserEvent,
  ImpressionStats,
  AlsoToOneContent,
  OnlyToOneContent,
  AlsoToMain,
  OnlyToMain,
  AlsoToPreloaded,
  SetPref,
  WebExtEvent,
  DiscoveryStreamImpressionStats,
  DiscoveryStreamLoadedContent
}; // These are helpers to test for certain kinds of actions

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
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PrefsButton", function() { return PrefsButton; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PersonalizeButton", function() { return PersonalizeButton; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Base", function() { return _Base; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "BaseContent", function() { return BaseContent; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Base", function() { return Base; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_ASRouterAdmin_ASRouterAdmin__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(4);
/* harmony import */ var _asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(10);
/* harmony import */ var content_src_components_ConfirmDialog_ConfirmDialog__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(15);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var content_src_components_DiscoveryStreamBase_DiscoveryStreamBase__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(16);
/* harmony import */ var content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(35);
/* harmony import */ var content_src_components_CustomizeMenu_CustomizeMenu__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(72);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_8__);
/* harmony import */ var content_src_components_Search_Search__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(65);
/* harmony import */ var content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(47);
/* harmony import */ var react_transition_group__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(29);
/* harmony import */ var react_transition_group__WEBPACK_IMPORTED_MODULE_11___default = /*#__PURE__*/__webpack_require__.n(react_transition_group__WEBPACK_IMPORTED_MODULE_11__);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */












const PrefsButton = ({
  onClick,
  icon
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
  className: "prefs-button"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("button", {
  className: `icon ${icon || "icon-settings"}`,
  onClick: onClick,
  "data-l10n-id": "newtab-settings-button"
}));
const PersonalizeButton = ({
  onClick
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("button", {
  className: "personalize-button",
  onClick: onClick,
  "data-l10n-id": "newtab-personalize-button-label"
}); // Returns a function will not be continuously triggered when called. The
// function will be triggered if called again after `wait` milliseconds.

function debounce(func, wait) {
  let timer;
  return (...args) => {
    if (timer) {
      return;
    }

    let wakeUp = () => {
      timer = null;
    };

    timer = setTimeout(wakeUp, wait);
    func.apply(this, args);
  };
}

class _Base extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      message: {}
    };
    this.notifyContent = this.notifyContent.bind(this);
  }

  notifyContent(state) {
    this.setState(state);
  }

  componentWillUnmount() {
    this.updateTheme();
  }

  componentWillUpdate() {
    this.updateTheme();
  }

  updateTheme() {
    const bodyClassName = ["activity-stream", // If we skipped the about:welcome overlay and removed the CSS classes
    // we don't want to add them back to the Activity Stream view
    document.body.classList.contains("inline-onboarding") ? "inline-onboarding" : ""].filter(v => v).join(" ");
    global.document.body.className = bodyClassName;
  }

  render() {
    const {
      props
    } = this;
    const {
      App
    } = props;
    const isDevtoolsEnabled = props.Prefs.values["asrouter.devtoolsEnabled"];

    if (!App.initialized) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_6__["ErrorBoundary"], {
      className: "base-content-fallback"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_8___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(BaseContent, _extends({}, this.props, {
      adminContent: this.state
    })), isDevtoolsEnabled ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ASRouterAdmin_ASRouterAdmin__WEBPACK_IMPORTED_MODULE_1__["ASRouterAdmin"], {
      notifyContent: this.notifyContent
    }) : null));
  }

}
class BaseContent extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
    this.openCustomizationMenu = this.openCustomizationMenu.bind(this);
    this.closeCustomizationMenu = this.closeCustomizationMenu.bind(this);
    this.handleOnKeyDown = this.handleOnKeyDown.bind(this);
    this.onWindowScroll = debounce(this.onWindowScroll.bind(this), 5);
    this.setPref = this.setPref.bind(this);
    this.state = {
      fixedSearch: false,
      customizeMenuVisible: false
    };
  }

  componentDidMount() {
    global.addEventListener("scroll", this.onWindowScroll);
    global.addEventListener("keydown", this.handleOnKeyDown);
  }

  componentWillUnmount() {
    global.removeEventListener("scroll", this.onWindowScroll);
    global.removeEventListener("keydown", this.handleOnKeyDown);
  }

  onWindowScroll() {
    const prefs = this.props.Prefs.values;
    const SCROLL_THRESHOLD = prefs["logowordmark.alwaysVisible"] ? 179 : 34;

    if (global.scrollY > SCROLL_THRESHOLD && !this.state.fixedSearch) {
      this.setState({
        fixedSearch: true
      });
    } else if (global.scrollY <= SCROLL_THRESHOLD && this.state.fixedSearch) {
      this.setState({
        fixedSearch: false
      });
    }
  }

  openPreferences() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SETTINGS_OPEN
    }));
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event: "OPEN_NEWTAB_PREFS"
    }));
  }

  openCustomizationMenu() {
    this.setState({
      customizeMenuVisible: true
    });
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event: "SHOW_PERSONALIZE"
    }));
  }

  closeCustomizationMenu() {
    if (this.state.customizeMenuVisible) {
      this.setState({
        customizeMenuVisible: false
      });
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "HIDE_PERSONALIZE"
      }));
    }
  }

  handleOnKeyDown(e) {
    if (e.key === "Escape") {
      this.closeCustomizationMenu();
    }
  }

  setPref(pref, value) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].SetPref(pref, value));
  }

  render() {
    const {
      props
    } = this;
    const {
      App
    } = props;
    const {
      initialized
    } = App;
    const prefs = props.Prefs.values; // Values from experiment data

    const {
      prefsButtonIcon
    } = prefs.featureConfig || {};
    const isDiscoveryStream = props.DiscoveryStream.config && props.DiscoveryStream.config.enabled;
    let filteredSections = props.Sections.filter(section => section.id !== "topstories");
    const pocketEnabled = prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"];
    const noSectionsEnabled = !prefs["feeds.topsites"] && !pocketEnabled && filteredSections.filter(section => section.enabled).length === 0;
    const searchHandoffEnabled = prefs["improvesearch.handoffToAwesomebar"];
    const {
      customizationMenuEnabled,
      newNewtabExperienceEnabled
    } = prefs.featureConfig || {};
    const canShowCustomizationMenu = customizationMenuEnabled || newNewtabExperienceEnabled;
    const showCustomizationMenu = canShowCustomizationMenu && this.state.customizeMenuVisible;
    const enabledSections = {
      topSitesEnabled: prefs["feeds.topsites"],
      pocketEnabled: prefs["feeds.section.topstories"],
      snippetsEnabled: prefs["feeds.snippets"],
      highlightsEnabled: prefs["feeds.section.highlights"],
      showSponsoredTopSitesEnabled: prefs.showSponsoredTopSites,
      showSponsoredPocketEnabled: prefs.showSponsored,
      topSitesRowsCount: prefs.topSitesRows
    };
    const pocketRegion = prefs["feeds.system.topstories"];
    const {
      mayHaveSponsoredTopSites
    } = prefs;
    const outerClassName = ["outer-wrapper", isDiscoveryStream && pocketEnabled && "ds-outer-wrapper-search-alignment", isDiscoveryStream && "ds-outer-wrapper-breakpoint-override", prefs.showSearch && this.state.fixedSearch && !noSectionsEnabled && "fixed-search", prefs.showSearch && noSectionsEnabled && "only-search", prefs["logowordmark.alwaysVisible"] && "visible-logo", newNewtabExperienceEnabled && "newtab-experience"].filter(v => v).join(" ");
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", null, canShowCustomizationMenu ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("span", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(PersonalizeButton, {
      onClick: this.openCustomizationMenu
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(react_transition_group__WEBPACK_IMPORTED_MODULE_11__["CSSTransition"], {
      timeout: 0,
      classNames: "customize-animate",
      in: showCustomizationMenu,
      appear: true
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_CustomizeMenu_CustomizeMenu__WEBPACK_IMPORTED_MODULE_7__["CustomizeMenu"], {
      onClose: this.closeCustomizationMenu,
      openPreferences: this.openPreferences,
      setPref: this.setPref,
      enabledSections: enabledSections,
      pocketRegion: pocketRegion,
      mayHaveSponsoredTopSites: mayHaveSponsoredTopSites
    }))) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(PrefsButton, {
      onClick: this.openPreferences,
      icon: prefsButtonIcon
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: outerClassName,
      onClick: this.closeCustomizationMenu
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("main", null, prefs.showSearch && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "non-collapsible-section"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_6__["ErrorBoundary"], null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Search_Search__WEBPACK_IMPORTED_MODULE_9__["Search"], _extends({
      showLogo: noSectionsEnabled || prefs["logowordmark.alwaysVisible"],
      handoffEnabled: searchHandoffEnabled
    }, props.Search)))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(_asrouter_asrouter_content__WEBPACK_IMPORTED_MODULE_2__["ASRouterUISurface"], {
      adminContent: this.props.adminContent,
      appUpdateChannel: this.props.Prefs.values.appUpdateChannel,
      fxaEndpoint: this.props.Prefs.values.fxa_endpoint,
      dispatch: this.props.dispatch
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: `body-wrapper${initialized ? " on" : ""}`
    }, isDiscoveryStream ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_6__["ErrorBoundary"], {
      className: "borderless-error"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_DiscoveryStreamBase_DiscoveryStreamBase__WEBPACK_IMPORTED_MODULE_5__["DiscoveryStreamBase"], {
      locale: props.App.locale
    })) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_10__["Sections"], null)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ConfirmDialog_ConfirmDialog__WEBPACK_IMPORTED_MODULE_3__["ConfirmDialog"], null))));
  }

}
const Base = Object(react_redux__WEBPACK_IMPORTED_MODULE_4__["connect"])(state => ({
  App: state.App,
  Prefs: state.Prefs,
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Search: state.Search
}))(_Base);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

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
	g = g || new Function("return this")();
} catch (e) {
	// This works if the window reference is available
	if (typeof window === "object") g = window;
}

// g can still be undefined, but nothing to do about it...
// We return undefined, instead of nothing here, so it's
// easier to handle this case. if(!global) { ...}

module.exports = g;


/***/ }),
/* 4 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ToggleStoryButton", function() { return ToggleStoryButton; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ToggleMessageJSON", function() { return ToggleMessageJSON; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TogglePrefCheckbox", function() { return TogglePrefCheckbox; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Personalization", function() { return Personalization; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DiscoveryStreamAdmin", function() { return DiscoveryStreamAdmin; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterAdminInner", function() { return ASRouterAdminInner; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CollapseToggle", function() { return CollapseToggle; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterAdmin", function() { return ASRouterAdmin; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(5);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var _SimpleHashRouter__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(9);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






const Row = props => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", _extends({
  className: "message-item"
}, props), props.children);

function relativeTime(timestamp) {
  if (!timestamp) {
    return "";
  }

  const seconds = Math.floor((Date.now() - timestamp) / 1000);
  const minutes = Math.floor((Date.now() - timestamp) / 60000);

  if (seconds < 2) {
    return "just now";
  } else if (seconds < 60) {
    return `${seconds} seconds ago`;
  } else if (minutes === 1) {
    return "1 minute ago";
  } else if (minutes < 600) {
    return `${minutes} minutes ago`;
  }

  return new Date(timestamp).toLocaleString();
}

const LAYOUT_VARIANTS = {
  basic: "Basic default layout (on by default in nightly)",
  staging_spocs: "A layout with all spocs shown",
  "dev-test-all": "A little bit of everything. Good layout for testing all components",
  "dev-test-feeds": "Stress testing for slow feeds"
};
class ToggleStoryButton extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.onClick(this.props.story);
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      onClick: this.handleClick
    }, "collapse/open");
  }

}
class ToggleMessageJSON extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.toggleJSON(this.props.msgId);
  }

  render() {
    let iconName = this.props.isCollapsed ? "icon icon-arrowhead-forward-small" : "icon icon-arrowhead-down-small";
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "clearButton",
      onClick: this.handleClick
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: iconName
    }));
  }

}
class TogglePrefCheckbox extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onChange = this.onChange.bind(this);
  }

  onChange(event) {
    this.props.onChange(this.props.pref, event.target.checked);
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "checkbox",
      checked: this.props.checked,
      onChange: this.onChange,
      disabled: this.props.disabled
    }), " ", this.props.pref, " ");
  }

}
class Personalization extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.togglePersonalizationVersion = this.togglePersonalizationVersion.bind(this);
  }

  togglePersonalizationVersion() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE
    }));
  }

  render() {
    const {
      lastUpdated,
      version,
      initialized
    } = this.props.state.Personalization;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.togglePersonalizationVersion
    }, version === 1 ? "Enable V2 Personalization" : "Enable V1 Personalization"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Personalization version"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, version)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Personalization Last Updated"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, relativeTime(lastUpdated) || "(no data)")), version === 2 ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Personalization V2 Initialized"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, initialized ? "true" : "false")) : null)));
  }

}
class DiscoveryStreamAdmin extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.restorePrefDefaults = this.restorePrefDefaults.bind(this);
    this.setConfigValue = this.setConfigValue.bind(this);
    this.expireCache = this.expireCache.bind(this);
    this.refreshCache = this.refreshCache.bind(this);
    this.idleDaily = this.idleDaily.bind(this);
    this.systemTick = this.systemTick.bind(this);
    this.syncRemoteSettings = this.syncRemoteSettings.bind(this);
    this.changeEndpointVariant = this.changeEndpointVariant.bind(this);
    this.onStoryToggle = this.onStoryToggle.bind(this);
    this.state = {
      toggledStories: {}
    };
  }

  setConfigValue(name, value) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_CONFIG_SET_VALUE,
      data: {
        name,
        value
      }
    }));
  }

  restorePrefDefaults(event) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS
    }));
  }

  refreshCache() {
    const {
      config
    } = this.props.state.DiscoveryStream;
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_CONFIG_CHANGE,
      data: config
    }));
  }

  dispatchSimpleAction(type) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type
    }));
  }

  systemTick() {
    this.dispatchSimpleAction(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_DEV_SYSTEM_TICK);
  }

  expireCache() {
    this.dispatchSimpleAction(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_DEV_EXPIRE_CACHE);
  }

  idleDaily() {
    this.dispatchSimpleAction(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_DEV_IDLE_DAILY);
  }

  syncRemoteSettings() {
    this.dispatchSimpleAction(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_DEV_SYNC_RS);
  }

  changeEndpointVariant(event) {
    const endpoint = this.props.state.DiscoveryStream.config.layout_endpoint;

    if (endpoint) {
      this.setConfigValue("layout_endpoint", endpoint.replace(/layout_variant=.+/, `layout_variant=${event.target.value}`));
    }
  }

  renderComponent(width, component) {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Type"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, component.type)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Width"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, width)), component.feed && this.renderFeed(component.feed)));
  }

  isCurrentVariant(id) {
    const endpoint = this.props.state.DiscoveryStream.config.layout_endpoint;
    const isMatch = endpoint && !!endpoint.match(`layout_variant=${id}`);
    return isMatch;
  }

  renderFeedData(url) {
    const {
      feeds
    } = this.props.state.DiscoveryStream;
    const feed = feeds.data[url].data;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h4", null, "Feed url: ", url), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, feed.recommendations.map(story => this.renderStoryData(story)))));
  }

  renderFeedsData() {
    const {
      feeds
    } = this.props.state.DiscoveryStream;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, Object.keys(feeds.data).map(url => this.renderFeedData(url)));
  }

  renderSpocs() {
    const {
      spocs
    } = this.props.state.DiscoveryStream;
    let spocsData = [];

    if (spocs.data && spocs.data.spocs && spocs.data.spocs.items) {
      spocsData = spocs.data.spocs.items || [];
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "spocs_endpoint"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, spocs.spocs_endpoint)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Data last fetched"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, relativeTime(spocs.lastUpdated))))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h4", null, "Spoc data"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, spocsData.map(spoc => this.renderStoryData(spoc)))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h4", null, "Spoc frequency caps"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, spocs.frequency_caps.map(spoc => this.renderStoryData(spoc)))));
  }

  onStoryToggle(story) {
    const {
      toggledStories
    } = this.state;
    this.setState({
      toggledStories: { ...toggledStories,
        [story.id]: !toggledStories[story.id]
      }
    });
  }

  renderStoryData(story) {
    let storyData = "";

    if (this.state.toggledStories[story.id]) {
      storyData = JSON.stringify(story, null, 2);
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: "message-item",
      key: story.id
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, story.id, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(ToggleStoryButton, {
      story: story,
      onClick: this.onStoryToggle
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "message-summary"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("pre", null, storyData)));
  }

  renderFeed(feed) {
    const {
      feeds
    } = this.props.state.DiscoveryStream;

    if (!feed.url) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Feed url"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, feed.url)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Data last fetched"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, relativeTime(feeds.data[feed.url] ? feeds.data[feed.url].lastUpdated : null) || "(no data)")));
  }

  render() {
    const prefToggles = "enabled hardcoded_layout show_spocs personalized collapsible".split(" ");
    const {
      config,
      lastUpdated,
      layout
    } = this.props.state.DiscoveryStream;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.restorePrefDefaults
    }, "Restore Pref Defaults"), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.refreshCache
    }, "Refresh Cache"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.expireCache
    }, "Expire Cache"), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.systemTick
    }, "Trigger System Tick"), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.idleDaily
    }, "Trigger Idle Daily"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button",
      onClick: this.syncRemoteSettings
    }, "Sync Remote Settings"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, prefToggles.map(pref => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, {
      key: pref
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(TogglePrefCheckbox, {
      checked: config[pref],
      pref: pref,
      onChange: this.setConfigValue
    })))))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Endpoint variant"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, "You can also change this manually by changing this pref:", " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("code", null, "browser.newtabpage.activity-stream.discoverystream.config")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", {
      style: config.enabled && !config.hardcoded_layout ? null : {
        opacity: 0.5
      }
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, Object.keys(LAYOUT_VARIANTS).map(id => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, {
      key: id
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "radio",
      value: id,
      checked: this.isCurrentVariant(id),
      onChange: this.changeEndpointVariant
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, id), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, LAYOUT_VARIANTS[id]))))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Caching info"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", {
      style: config.enabled ? null : {
        opacity: 0.5
      }
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Data last fetched"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, relativeTime(lastUpdated) || "(no data)")))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Layout"), layout.map((row, rowIndex) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      key: `row-${rowIndex}`
    }, row.components.map((component, componentIndex) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      key: `component-${componentIndex}`,
      className: "ds-component"
    }, this.renderComponent(row.width, component))))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Personalization"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Personalization, {
      dispatch: this.props.dispatch,
      state: {
        Personalization: this.props.state.Personalization
      }
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Spocs"), this.renderSpocs(), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", null, "Feeds Data"), this.renderFeedsData());
  }

}
class ASRouterAdminInner extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.handleEnabledToggle = this.handleEnabledToggle.bind(this);
    this.handleUserPrefToggle = this.handleUserPrefToggle.bind(this);
    this.onChangeMessageFilter = this.onChangeMessageFilter.bind(this);
    this.onChangeMessageGroupsFilter = this.onChangeMessageGroupsFilter.bind(this);
    this.unblockAll = this.unblockAll.bind(this);
    this.handleClearAllImpressionsByProvider = this.handleClearAllImpressionsByProvider.bind(this);
    this.handleExpressionEval = this.handleExpressionEval.bind(this);
    this.onChangeTargetingParameters = this.onChangeTargetingParameters.bind(this);
    this.onChangeAttributionParameters = this.onChangeAttributionParameters.bind(this);
    this.setAttribution = this.setAttribution.bind(this);
    this.onCopyTargetingParams = this.onCopyTargetingParams.bind(this);
    this.onNewTargetingParams = this.onNewTargetingParams.bind(this);
    this.handleUpdateWNMessages = this.handleUpdateWNMessages.bind(this);
    this.handleForceWNP = this.handleForceWNP.bind(this);
    this.handleCloseWNP = this.handleCloseWNP.bind(this);
    this.resetPanel = this.resetPanel.bind(this);
    this.restoreWNMessageState = this.restoreWNMessageState.bind(this);
    this.toggleJSON = this.toggleJSON.bind(this);
    this.toggleAllMessages = this.toggleAllMessages.bind(this);
    this.resetGroups = this.resetGroups.bind(this);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.setStateFromParent = this.setStateFromParent.bind(this);
    this.setState = this.setState.bind(this);
    this.state = {
      messageFilter: "all",
      messageGroupsFilter: "all",
      WNMessages: [],
      collapsedMessages: [],
      modifiedMessages: [],
      evaluationStatus: {},
      stringTargetingParameters: null,
      newStringTargetingParameters: null,
      copiedToClipboard: false,
      attributionParameters: {
        source: "addons.mozilla.org",
        medium: "referral",
        campaign: "non-fx-button",
        content: "iridium@particlecore.github.io",
        experiment: "ua-onboarding",
        variation: "chrome",
        ua: "Google Chrome 123",
        dltoken: "00000000-0000-0000-0000-000000000000"
      }
    };
  }

  onMessageFromParent({
    type,
    data
  }) {
    // These only exists due to onPrefChange events in ASRouter
    switch (type) {
      case "UpdateAdminState":
        {
          this.setStateFromParent(data);
          break;
        }
    }
  }

  setStateFromParent(data) {
    this.setState(data);

    if (!this.state.stringTargetingParameters) {
      const stringTargetingParameters = {};

      for (const param of Object.keys(data.targetingParameters)) {
        stringTargetingParameters[param] = JSON.stringify(data.targetingParameters[param], null, 2);
      }

      this.setState({
        stringTargetingParameters
      });
    }
  }

  componentWillMount() {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].addListener(this.onMessageFromParent);
    const endpoint = _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].getPreviewEndpoint();
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "ADMIN_CONNECT_STATE",
      data: {
        endpoint
      }
    }).then(this.setStateFromParent);
  }

  handleBlock(msg) {
    return () => _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].blockById(msg.id);
  }

  handleUnblock(msg) {
    return () => _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].unblockById(msg.id);
  }

  resetJSON(msg) {
    // reset the displayed JSON for the given message
    document.getElementById(`${msg.id}-textarea`).value = JSON.stringify(msg, null, 2); // remove the message from the list of modified IDs

    let index = this.state.modifiedMessages.indexOf(msg.id);
    this.setState(prevState => ({
      modifiedMessages: [...prevState.modifiedMessages.slice(0, index), ...prevState.modifiedMessages.slice(index + 1)]
    }));
  }

  resetAllJSON() {
    let messageCheckboxes = document.querySelectorAll('input[type="checkbox"]');

    for (const checkbox of messageCheckboxes) {
      let trimmedId = checkbox.id.replace(" checkbox", "");
      let message = this.state.messages.filter(msg => msg.id === trimmedId);
      let msgId = message[0].id;
      document.getElementById(`${msgId}-textarea`).value = JSON.stringify(message[0], null, 2);
    }

    this.setState({
      WNMessages: []
    });
  }

  resetPanel() {
    this.resetAllJSON();
    this.handleCloseWNP();
  }

  handleOverride(id) {
    return () => _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].overrideMessage(id).then(state => {
      this.setStateFromParent(state);
      this.props.notifyContent({
        message: state.message
      });
    });
  }

  async handleUpdateWNMessages() {
    await this.restoreWNMessageState();
    let messages = this.state.WNMessages;

    for (const msg of messages) {
      _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].modifyMessageJson(JSON.parse(msg));
    }
  }

  handleForceWNP() {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "FORCE_WHATSNEW_PANEL"
    });
  }

  handleCloseWNP() {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "CLOSE_WHATSNEW_PANEL"
    });
  }

  expireCache() {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "EXPIRE_QUERY_CACHE"
    });
  }

  resetPref() {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "RESET_PROVIDER_PREF"
    });
  }

  resetGroups(id, value) {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "RESET_GROUPS_STATE"
    }).then(this.setStateFromParent);
  }

  handleExpressionEval() {
    const context = {};

    for (const param of Object.keys(this.state.stringTargetingParameters)) {
      const value = this.state.stringTargetingParameters[param];
      context[param] = value ? JSON.parse(value) : null;
    }

    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "EVALUATE_JEXL_EXPRESSION",
      data: {
        expression: this.refs.expressionInput.value,
        context
      }
    }).then(this.setStateFromParent);
  }

  onChangeTargetingParameters(event) {
    const {
      name
    } = event.target;
    const {
      value
    } = event.target;
    this.setState(({
      stringTargetingParameters
    }) => {
      let targetingParametersError = null;
      const updatedParameters = { ...stringTargetingParameters
      };
      updatedParameters[name] = value;

      try {
        JSON.parse(value);
      } catch (e) {
        console.log(`Error parsing value of parameter ${name}`); // eslint-disable-line no-console

        targetingParametersError = {
          id: name
        };
      }

      return {
        copiedToClipboard: false,
        evaluationStatus: {},
        stringTargetingParameters: updatedParameters,
        targetingParametersError
      };
    });
  }

  unblockAll() {
    return _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "UNBLOCK_ALL"
    }).then(this.setStateFromParent);
  }

  handleClearAllImpressionsByProvider() {
    const providerId = this.state.messageFilter;

    if (!providerId) {
      return;
    }

    const userPrefInfo = this.state.userPrefs;
    const isUserEnabled = providerId in userPrefInfo ? userPrefInfo[providerId] : true;
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "DISABLE_PROVIDER",
      data: providerId
    });

    if (!isUserEnabled) {
      _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
        type: "SET_PROVIDER_USER_PREF",
        data: {
          id: providerId,
          value: true
        }
      });
    }

    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "ENABLE_PROVIDER",
      data: providerId
    });
  }

  handleEnabledToggle(event) {
    const provider = this.state.providerPrefs.find(p => p.id === event.target.dataset.provider);
    const userPrefInfo = this.state.userPrefs;
    const isUserEnabled = provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
    const isSystemEnabled = provider.enabled;
    const isEnabling = event.target.checked;

    if (isEnabling) {
      if (!isUserEnabled) {
        _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
          type: "SET_PROVIDER_USER_PREF",
          data: {
            id: provider.id,
            value: true
          }
        });
      }

      if (!isSystemEnabled) {
        _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
          type: "ENABLE_PROVIDER",
          data: provider.id
        });
      }
    } else {
      _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
        type: "DISABLE_PROVIDER",
        data: provider.id
      });
    }

    this.setState({
      messageFilter: "all"
    });
  }

  handleUserPrefToggle(event) {
    const action = {
      type: "SET_PROVIDER_USER_PREF",
      data: {
        id: event.target.dataset.provider,
        value: event.target.checked
      }
    };
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage(action);
    this.setState({
      messageFilter: "all"
    });
  }

  onChangeMessageFilter(event) {
    this.setState({
      messageFilter: event.target.value
    });
  }

  onChangeMessageGroupsFilter(event) {
    this.setState({
      messageGroupsFilter: event.target.value
    });
  } // Simulate a copy event that sets to clipboard all targeting paramters and values


  onCopyTargetingParams(event) {
    const stringTargetingParameters = { ...this.state.stringTargetingParameters
    };

    for (const key of Object.keys(stringTargetingParameters)) {
      // If the value is not set the parameter will be lost when we stringify
      if (stringTargetingParameters[key] === undefined) {
        stringTargetingParameters[key] = null;
      }
    }

    const setClipboardData = e => {
      e.preventDefault();
      e.clipboardData.setData("text", JSON.stringify(stringTargetingParameters, null, 2));
      document.removeEventListener("copy", setClipboardData);
      this.setState({
        copiedToClipboard: true
      });
    };

    document.addEventListener("copy", setClipboardData);
    document.execCommand("copy");
  }

  onNewTargetingParams(event) {
    this.setState({
      newStringTargetingParameters: event.target.value
    });
    event.target.classList.remove("errorState");
    this.refs.targetingParamsEval.innerText = "";

    try {
      const stringTargetingParameters = JSON.parse(event.target.value);
      this.setState({
        stringTargetingParameters
      });
    } catch (e) {
      event.target.classList.add("errorState");
      this.refs.targetingParamsEval.innerText = e.message;
    }
  }

  toggleJSON(msgId) {
    if (this.state.collapsedMessages.includes(msgId)) {
      let index = this.state.collapsedMessages.indexOf(msgId);
      this.setState(prevState => ({
        collapsedMessages: [...prevState.collapsedMessages.slice(0, index), ...prevState.collapsedMessages.slice(index + 1)]
      }));
    } else {
      this.setState(prevState => ({
        collapsedMessages: prevState.collapsedMessages.concat(msgId)
      }));
    }
  }

  handleChange(msgId) {
    if (!this.state.modifiedMessages.includes(msgId)) {
      this.setState(prevState => ({
        modifiedMessages: prevState.modifiedMessages.concat(msgId)
      }));
    }
  }

  renderMessageItem(msg) {
    const isBlockedByGroup = this.state.groups.filter(group => msg.groups.includes(group.id)).some(group => !group.enabled);
    const msgProvider = this.state.providers.find(provider => provider.id === msg.provider) || {};
    const isProviderExcluded = msgProvider.exclude && msgProvider.exclude.includes(msg.id);
    const isMessageBlocked = this.state.messageBlockList.includes(msg.id) || this.state.messageBlockList.includes(msg.campaign);
    const isBlocked = isMessageBlocked || isBlockedByGroup || isProviderExcluded;
    const impressions = this.state.messageImpressions[msg.id] ? this.state.messageImpressions[msg.id].length : 0;
    const isCollapsed = this.state.collapsedMessages.includes(msg.id);
    const isModified = this.state.modifiedMessages.includes(msg.id);
    let itemClassName = "message-item";

    if (isBlocked) {
      itemClassName += " blocked";
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: itemClassName,
      key: `${msg.id}-${msg.provider}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, msg.id, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(ToggleMessageJSON, {
      msgId: `${msg.id}`,
      toggleJSON: this.toggleJSON,
      isCollapsed: isCollapsed
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "button-column"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: `button ${isBlocked ? "" : " primary"}`,
      onClick: isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)
    }, isBlocked ? "Unblock" : "Block"), // eslint-disable-next-line no-nested-ternary
    isBlocked ? null : isModified ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button restore" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.resetJSON(msg)
    }, "Reset") : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button show",
      onClick: this.handleOverride(msg.id)
    }, "Show"), isBlocked ? null : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button modify" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.modifyJson(msg)
    }, "Modify"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), "(", impressions, " impressions)"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "message-summary"
    }, isBlocked && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, "Block reason:", isBlockedByGroup && " Blocked by group", isProviderExcluded && " Excluded by provider", isMessageBlocked && " Message blocked"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("pre", {
      className: isCollapsed ? "collapsed" : "expanded"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("textarea", {
      id: `${msg.id}-textarea`,
      name: msg.id,
      className: "general-textarea",
      disabled: isBlocked // eslint-disable-next-line react/jsx-no-bind
      ,
      onChange: e => this.handleChange(msg.id)
    }, JSON.stringify(msg, null, 2))))));
  }

  restoreWNMessageState() {
    // check the page for checked boxes, and reset the state of WNMessages based on that.
    let tempState = [];
    let messageCheckboxes = document.querySelectorAll('input[type="checkbox"]'); // put the JSON of all the checked checkboxes in the array

    for (const checkbox of messageCheckboxes) {
      let trimmedId = checkbox.id.replace(" checkbox", "");
      let msg = document.getElementById(`${trimmedId}-textarea`).value;

      if (checkbox.checked) {
        tempState.push(msg);
      }
    }

    this.setState({
      WNMessages: tempState
    });
  }

  modifyJson(content) {
    const message = JSON.parse(document.getElementById(`${content.id}-textarea`).value);
    return _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].modifyMessageJson(message).then(state => {
      this.setStateFromParent(state);
      this.props.notifyContent({
        message: state.message
      });
    });
  }

  renderWNMessageItem(msg) {
    const isBlocked = this.state.messageBlockList.includes(msg.id) || this.state.messageBlockList.includes(msg.campaign);
    const impressions = this.state.messageImpressions[msg.id] ? this.state.messageImpressions[msg.id].length : 0;
    const isCollapsed = this.state.collapsedMessages.includes(msg.id);
    let itemClassName = "message-item";

    if (isBlocked) {
      itemClassName += " blocked";
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: itemClassName,
      key: `${msg.id}-${msg.provider}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, msg.id, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), "(", impressions, " impressions)")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(ToggleMessageJSON, {
      msgId: `${msg.id}`,
      toggleJSON: this.toggleJSON,
      isCollapsed: isCollapsed
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "checkbox",
      id: `${msg.id} checkbox`,
      name: `${msg.id} checkbox`
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: `message-summary`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("pre", {
      className: isCollapsed ? "collapsed" : "expanded"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("textarea", {
      id: `${msg.id}-textarea`,
      className: "wnp-textarea",
      name: msg.id
    }, JSON.stringify(msg, null, 2)))));
  }

  toggleAllMessages(messagesToShow) {
    if (this.state.collapsedMessages.length) {
      this.setState({
        collapsedMessages: []
      });
    } else {
      Array.prototype.forEach.call(messagesToShow, msg => {
        this.setState(prevState => ({
          collapsedMessages: prevState.collapsedMessages.concat(msg.id)
        }));
      });
    }
  }

  renderMessages() {
    if (!this.state.messages) {
      return null;
    }

    const messagesToShow = this.state.messageFilter === "all" ? this.state.messages : this.state.messages.filter(message => message.provider === this.state.messageFilter);
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton slim" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.toggleAllMessages(messagesToShow)
    }, "Collapse/Expand All"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, "To modify a message, change the JSON and click 'Modify' to see your changes. Click 'Reset' to restore the JSON to the original.")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, messagesToShow.map(msg => this.renderMessageItem(msg)))));
  }

  renderMessagesByGroup() {
    if (!this.state.messages) {
      return null;
    }

    const messagesToShow = this.state.messageGroupsFilter === "all" ? this.state.messages.filter(m => m.groups.length) : this.state.messages.filter(message => message.groups.includes(this.state.messageGroupsFilter));
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, messagesToShow.map(msg => this.renderMessageItem(msg))));
  }

  renderWNMessages() {
    if (!this.state.messages) {
      return null;
    }

    const messagesToShow = this.state.messages.filter(message => message.provider === "whats-new-panel" && message.content.body);
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, messagesToShow.map(msg => this.renderWNMessageItem(msg))));
  }

  renderMessageFilter() {
    if (!this.state.providers) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "unblock-all ASRouterButton test-only",
      onClick: this.unblockAll
    }, "Unblock All Snippets"), "Show messages from ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("select", {
      value: this.state.messageFilter,
      onChange: this.onChangeMessageFilter
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("option", {
      value: "all"
    }, "all providers"), this.state.providers.map(provider => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("option", {
      key: provider.id,
      value: provider.id
    }, provider.id))), this.state.messageFilter !== "all" && !this.state.messageFilter.includes("_local_testing") ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "button messages-reset",
      onClick: this.handleClearAllImpressionsByProvider
    }, "Reset All") : null);
  }

  renderMessageGroupsFilter() {
    if (!this.state.groups) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, "Show messages from ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("select", {
      value: this.state.messageGroupsFilter,
      onChange: this.onChangeMessageGroupsFilter
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("option", {
      value: "all"
    }, "all groups"), this.state.groups.map(group => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("option", {
      key: group.id,
      value: group.id
    }, group.id))));
  }

  renderTableHead() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("thead", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: "message-item"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Provider ID"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, "Source"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Cohort"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Last Updated")));
  }

  renderProviders() {
    const providersConfig = this.state.providerPrefs;
    const providerInfo = this.state.providers;
    const userPrefInfo = this.state.userPrefs;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, this.renderTableHead(), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, providersConfig.map((provider, i) => {
      const isTestProvider = provider.id.includes("_local_testing");
      const info = providerInfo.find(p => p.id === provider.id) || {};
      const isUserEnabled = provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
      const isSystemEnabled = isTestProvider || provider.enabled;
      let label = "local";

      if (provider.type === "remote") {
        label = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, "endpoint (", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
          className: "providerUrl",
          target: "_blank",
          href: info.url,
          rel: "noopener noreferrer"
        }, info.url), ")");
      } else if (provider.type === "remote-settings") {
        label = `remote settings (${provider.bucket})`;
      } else if (provider.type === "remote-experiments") {
        label = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, "remote settings (", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
          className: "providerUrl",
          target: "_blank",
          href: "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/nimbus-desktop-experiments/records",
          rel: "noopener noreferrer"
        }, "nimbus-desktop-experiments"), ")");
      }

      let reasonsDisabled = [];

      if (!isSystemEnabled) {
        reasonsDisabled.push("system pref");
      }

      if (!isUserEnabled) {
        reasonsDisabled.push("user pref");
      }

      if (reasonsDisabled.length) {
        label = `disabled via ${reasonsDisabled.join(", ")}`;
      }

      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
        className: "message-item",
        key: i
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, isTestProvider ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
        type: "checkbox",
        disabled: true,
        readOnly: true,
        checked: true
      }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
        type: "checkbox",
        "data-provider": provider.id,
        checked: isUserEnabled && isSystemEnabled,
        onChange: this.handleEnabledToggle
      })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, provider.id), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: `sourceLabel${isUserEnabled && isSystemEnabled ? "" : " isDisabled"}`
      }, label)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, provider.cohort), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
        style: {
          whiteSpace: "nowrap"
        }
      }, info.lastUpdated ? new Date(info.lastUpdated).toLocaleString() : ""));
    })));
  }

  renderTargetingParameters() {
    // There was no error and the result is truthy
    const success = this.state.evaluationStatus.success && !!this.state.evaluationStatus.result;
    const result = JSON.stringify(this.state.evaluationStatus.result, null, 2) || "(Empty result)";
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Evaluate JEXL expression"))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("textarea", {
      ref: "expressionInput",
      rows: "10",
      cols: "60",
      placeholder: "Evaluate JEXL expressions and mock parameters by changing their values below"
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, "Status:", " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      ref: "evaluationStatus"
    }, success ? "✅" : "❌", ", Result: ", result))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton secondary",
      onClick: this.handleExpressionEval
    }, "Evaluate"))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Modify targeting parameters"))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton secondary",
      onClick: this.onCopyTargetingParams,
      disabled: this.state.copiedToClipboard
    }, this.state.copiedToClipboard ? "Parameters copied!" : "Copy parameters"))), this.state.stringTargetingParameters && Object.keys(this.state.stringTargetingParameters).map((param, i) => {
      const value = this.state.stringTargetingParameters[param];
      const errorState = this.state.targetingParametersError && this.state.targetingParametersError.id === param;
      const className = errorState ? "errorState" : "";
      const inputComp = (value && value.length) > 30 ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("textarea", {
        name: param,
        className: className,
        value: value,
        rows: "10",
        cols: "60",
        onChange: this.onChangeTargetingParameters
      }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
        name: param,
        className: className,
        value: value,
        onChange: this.onChangeTargetingParameters
      });
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
        key: i
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, param), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, inputComp));
    })));
  }

  onChangeAttributionParameters(event) {
    const {
      name,
      value
    } = event.target;
    this.setState(({
      attributionParameters
    }) => {
      const updatedParameters = { ...attributionParameters
      };
      updatedParameters[name] = value;
      return {
        attributionParameters: updatedParameters
      };
    });
  }

  setAttribution(e) {
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].sendMessage({
      type: "FORCE_ATTRIBUTION",
      data: this.state.attributionParameters
    }).then(this.setStateFromParent);
  }

  _getGroupImpressionsCount(id, frequency) {
    if (frequency) {
      return this.state.groupImpressions[id] ? this.state.groupImpressions[id].length : 0;
    }

    return "n/a";
  }

  renderDiscoveryStream() {
    const {
      config
    } = this.props.DiscoveryStream;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: "message-item"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Enabled"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, config.enabled ? "yes" : "no")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      className: "message-item"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      className: "min"
    }, "Endpoint"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, config.endpoint || "(empty)")))));
  }

  renderAttributionParamers() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, " Attribution Parameters "), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, " ", "This forces the browser to set some attribution parameters, useful for testing the Return To AMO feature. Clicking on 'Force Attribution', with the default values in each field, will demo the Return To AMO flow with the addon called 'Iridium for Youtube'. If you wish to try different attribution parameters, enter them in the text boxes. If you wish to try a different addon with the Return To AMO flow, make sure the 'content' text box has the addon GUID, then click 'Force Attribution'. Clicking on 'Force Attribution' with blank text boxes reset attribution data."), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Source ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "source",
      placeholder: "addons.mozilla.org",
      value: this.state.attributionParameters.source,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Medium ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "medium",
      placeholder: "referral",
      value: this.state.attributionParameters.medium,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Campaign ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "campaign",
      placeholder: "non-fx-button",
      value: this.state.attributionParameters.campaign,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Content ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "content",
      placeholder: "iridium@particlecore.github.io",
      value: this.state.attributionParameters.content,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Experiment ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "experiment",
      placeholder: "ua-onboarding",
      value: this.state.attributionParameters.experiment,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Variation ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "variation",
      placeholder: "chrome",
      value: this.state.attributionParameters.variation,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " User Agent ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "ua",
      placeholder: "Google Chrome 123",
      value: this.state.attributionParameters.ua,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("b", null, " Download Token ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "text",
      name: "dltoken",
      placeholder: "00000000-0000-0000-0000-000000000000",
      value: this.state.attributionParameters.dltoken,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton primary button",
      onClick: this.setAttribution
    }, " ", "Force Attribution", " "), " "))));
  }

  renderErrorMessage({
    id,
    errors
  }) {
    const providerId = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", {
      rowSpan: errors.length
    }, id); // .reverse() so that the last error (most recent) is first

    return errors.map(({
      error,
      timestamp
    }, cellKey) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
      key: cellKey
    }, cellKey === errors.length - 1 ? providerId : null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, error.message), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, relativeTime(timestamp)))).reverse();
  }

  renderErrors() {
    const providersWithErrors = this.state.providers && this.state.providers.filter(p => p.errors && p.errors.length);

    if (providersWithErrors && providersWithErrors.length) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", {
        className: "errorReporting"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("thead", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("th", null, "Provider ID"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("th", null, "Message"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("th", null, "Timestamp"))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, providersWithErrors.map(this.renderErrorMessage)));
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", null, "No errors");
  }

  renderWNPTests() {
    if (!this.state.messages) {
      return null;
    }

    let messagesToShow = this.state.messages.filter(message => message.provider === "whats-new-panel");
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, "To correctly render selected messages, click 'Open What's New Panel', select the messages you want to see, and click 'Render Selected Messages'.", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), "To modify a message, select it, modify the JSON and click 'Render Selected Messages' again to see your changes.", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("br", null), "Click 'Reset Panel' to close the panel and reset all JSON to its original state.")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton primary button",
      onClick: this.handleForceWNP
    }, "Open What's New Panel"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton secondary button",
      onClick: this.handleUpdateWNMessages
    }, "Render Selected Messages"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton secondary button",
      onClick: this.resetPanel
    }, "Reset Panel"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Messages"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      className: "ASRouterButton slim button" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.toggleAllMessages(messagesToShow)
    }, "Collapse/Expand All"), this.renderWNMessages()));
  }

  getSection() {
    const [section] = this.props.location.routes;

    switch (section) {
      case "wnpanel":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "What's New Panel"), this.renderWNPTests());

      case "targeting":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Targeting Utilities"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
          className: "button",
          onClick: this.expireCache
        }, "Expire Cache"), " ", "(This expires the cache in ASR Targeting for bookmarks and top sites)", this.renderTargetingParameters(), this.renderAttributionParamers());

      case "groups":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Message Groups"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
          className: "button",
          onClick: this.resetGroups
        }, "Reset group impressions"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("table", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("thead", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tr", {
          className: "message-item"
        }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, "Enabled"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, "Impressions count"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, "Custom frequency"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, "User preferences"))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("tbody", null, this.state.groups && this.state.groups.map(({
          id,
          enabled,
          frequency,
          userPreferences = []
        }, index) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(Row, {
          key: id
        }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(TogglePrefCheckbox, {
          checked: enabled,
          pref: id,
          disabled: true
        })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, this._getGroupImpressionsCount(id, frequency)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, JSON.stringify(frequency, null, 2)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("td", null, userPreferences.join(", ")))))), this.renderMessageGroupsFilter(), this.renderMessagesByGroup());

      case "ds":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Discovery Stream"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(DiscoveryStreamAdmin, {
          state: {
            DiscoveryStream: this.props.DiscoveryStream,
            Personalization: this.props.Personalization
          },
          otherPrefs: this.props.Prefs.values,
          dispatch: this.props.dispatch
        }));

      case "errors":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "ASRouter Errors"), this.renderErrors());

      default:
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Message Providers", " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
          title: "Restore all provider settings that ship with Firefox",
          className: "button",
          onClick: this.resetPref
        }, "Restore default prefs")), this.state.providers ? this.renderProviders() : null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h2", null, "Messages"), this.renderMessageFilter(), this.renderMessages());
    }
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: `asrouter-admin ${this.props.collapsed ? "collapsed" : "expanded"}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("aside", {
      className: "sidebar"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("ul", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools"
    }, "General")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools-wnpanel"
    }, "What's New Panel")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools-targeting"
    }, "Targeting")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools-groups"
    }, "Message Groups")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools-ds"
    }, "Discovery Stream")), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools-errors"
    }, "Errors")))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("main", {
      className: "main-panel"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h1", null, "AS Router Admin"), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", null, "Need help using these tools? Check out our", " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      target: "blank",
      href: "https://github.com/mozilla/activity-stream/blob/master/content-src/asrouter/docs/debugging-docs.md"
    }, "documentation"))), this.getSection()));
  }

}
class CollapseToggle extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onCollapseToggle = this.onCollapseToggle.bind(this);
    this.state = {
      collapsed: false
    };
  }

  get renderAdmin() {
    const {
      props
    } = this;
    return props.location.hash && (props.location.hash.startsWith("#asrouter") || props.location.hash.startsWith("#devtools"));
  }

  onCollapseToggle(e) {
    e.preventDefault();
    this.setState(state => ({
      collapsed: !state.collapsed
    }));
  }

  setBodyClass() {
    if (this.renderAdmin && !this.state.collapsed) {
      global.document.body.classList.add("no-scroll");
    } else {
      global.document.body.classList.remove("no-scroll");
    }
  }

  componentDidMount() {
    this.setBodyClass();
  }

  componentDidUpdate() {
    this.setBodyClass();
  }

  componentWillUnmount() {
    global.document.body.classList.remove("no-scroll");
    _asrouter_asrouter_utils__WEBPACK_IMPORTED_MODULE_1__["ASRouterUtils"].removeListener(this.onMessageFromParent);
  }

  render() {
    const {
      props
    } = this;
    const {
      renderAdmin
    } = this;
    const isCollapsed = this.state.collapsed || !renderAdmin;
    const label = `${isCollapsed ? "Expand" : "Collapse"} devtools`;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_3___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
      href: "#devtools",
      title: label,
      "aria-label": label,
      className: `asrouter-toggle ${isCollapsed ? "collapsed" : "expanded"}`,
      onClick: this.renderAdmin ? this.onCollapseToggle : null
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: "icon icon-devtools"
    })), renderAdmin ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(ASRouterAdminInner, _extends({}, props, {
      collapsed: this.state.collapsed
    })) : null);
  }

}

const _ASRouterAdmin = props => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_SimpleHashRouter__WEBPACK_IMPORTED_MODULE_4__["SimpleHashRouter"], null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(CollapseToggle, props));

const ASRouterAdmin = Object(react_redux__WEBPACK_IMPORTED_MODULE_2__["connect"])(state => ({
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Personalization: state.Personalization,
  Prefs: state.Prefs
}))(_ASRouterAdmin);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 5 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterUtils", function() { return ASRouterUtils; });
/* harmony import */ var common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(1);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const ASRouterUtils = {
  addListener(listener) {
    if (global.ASRouterAddParentListener) {
      global.ASRouterAddParentListener(listener);
    }
  },

  removeListener(listener) {
    if (global.ASRouterRemoveParentListener) {
      global.ASRouterRemoveParentListener(listener);
    }
  },

  sendMessage(action) {
    if (global.ASRouterMessage) {
      return global.ASRouterMessage(action);
    }

    throw new Error(`Unexpected call:\n${JSON.stringify(action, null, 3)}`);
  },

  blockById(id, options) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].BLOCK_MESSAGE_BY_ID,
      data: {
        id,
        ...options
      }
    });
  },

  modifyMessageJson(content) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].MODIFY_MESSAGE_JSON,
      data: {
        content
      }
    });
  },

  executeAction(button_action) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].USER_ACTION,
      data: button_action
    });
  },

  unblockById(id) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].UNBLOCK_MESSAGE_BY_ID,
      data: {
        id
      }
    });
  },

  blockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].BLOCK_BUNDLE,
      data: {
        bundle
      }
    });
  },

  unblockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].UNBLOCK_BUNDLE,
      data: {
        bundle
      }
    });
  },

  overrideMessage(id) {
    return ASRouterUtils.sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].OVERRIDE_MESSAGE,
      data: {
        id
      }
    });
  },

  sendTelemetry(ping) {
    return ASRouterUtils.sendMessage(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].ASRouterUserEvent(ping));
  },

  getPreviewEndpoint() {
    if (global.document && global.document.location && global.document.location.href.includes("endpoint")) {
      const params = new URLSearchParams(global.document.location.href.slice(global.document.location.href.indexOf("endpoint")));

      try {
        const endpoint = new URL(params.get("endpoint"));
        return {
          url: endpoint.href,
          snippetId: params.get("snippetId"),
          theme: this.getPreviewTheme(),
          dir: this.getPreviewDir()
        };
      } catch (e) {}
    }

    return null;
  },

  getPreviewTheme() {
    return new URLSearchParams(global.document.location.href.slice(global.document.location.href.indexOf("theme"))).get("theme");
  },

  getPreviewDir() {
    return new URLSearchParams(global.document.location.href.slice(global.document.location.href.indexOf("dir"))).get("dir");
  }

};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 6 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MESSAGE_TYPE_LIST", function() { return MESSAGE_TYPE_LIST; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MESSAGE_TYPE_HASH", function() { return MESSAGE_TYPE_HASH; });
/* vim: set ts=2 sw=2 sts=2 et tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const MESSAGE_TYPE_LIST = ["BLOCK_MESSAGE_BY_ID", "USER_ACTION", "IMPRESSION", "TRIGGER", "NEWTAB_MESSAGE_REQUEST", "DOORHANGER_TELEMETRY", "TOOLBAR_BADGE_TELEMETRY", "TOOLBAR_PANEL_TELEMETRY", "MOMENTS_PAGE_TELEMETRY", "INFOBAR_TELEMETRY", "AS_ROUTER_TELEMETRY_USER_EVENT", // Admin types
"ADMIN_CONNECT_STATE", "UNBLOCK_MESSAGE_BY_ID", "UNBLOCK_ALL", "BLOCK_BUNDLE", "UNBLOCK_BUNDLE", "DISABLE_PROVIDER", "ENABLE_PROVIDER", "EVALUATE_JEXL_EXPRESSION", "EXPIRE_QUERY_CACHE", "FORCE_ATTRIBUTION", "FORCE_WHATSNEW_PANEL", "CLOSE_WHATSNEW_PANEL", "OVERRIDE_MESSAGE", "MODIFY_MESSAGE_JSON", "RESET_PROVIDER_PREF", "SET_PROVIDER_USER_PREF", "RESET_GROUPS_STATE"];
const MESSAGE_TYPE_HASH = MESSAGE_TYPE_LIST.reduce((hash, value) => {
  hash[value] = value;
  return hash;
}, {});

/***/ }),
/* 7 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 8 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 9 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SimpleHashRouter", function() { return SimpleHashRouter; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SimpleHashRouter extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onHashChange = this.onHashChange.bind(this);
    this.state = {
      hash: global.location.hash
    };
  }

  onHashChange() {
    this.setState({
      hash: global.location.hash
    });
  }

  componentWillMount() {
    global.addEventListener("hashchange", this.onHashChange);
  }

  componentWillUnmount() {
    global.removeEventListener("hashchange", this.onHashChange);
  }

  render() {
    const [, ...routes] = this.state.hash.split("-");
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.cloneElement(this.props.children, {
      location: {
        hash: this.state.hash,
        routes
      }
    });
  }

}
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 10 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ASRouterUISurface", function() { return ASRouterUISurface; });
/* harmony import */ var common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(1);
/* harmony import */ var _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(5);
/* harmony import */ var _rich_text_strings__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(71);
/* harmony import */ var _components_ImpressionsWrapper_ImpressionsWrapper__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(11);
/* harmony import */ var fluent_react__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(69);
/* harmony import */ var content_src_lib_constants__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(13);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_7___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_7__);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(14);
/* harmony import */ var react_dom__WEBPACK_IMPORTED_MODULE_8___default = /*#__PURE__*/__webpack_require__.n(react_dom__WEBPACK_IMPORTED_MODULE_8__);
/* harmony import */ var _templates_template_manifest__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(70);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */










const TEMPLATES_BELOW_SEARCH = ["simple_below_search_snippet"]; // Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />

function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id);
}

class ASRouterUISurface extends react__WEBPACK_IMPORTED_MODULE_7___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.sendClick = this.sendClick.bind(this);
    this.sendImpression = this.sendImpression.bind(this);
    this.sendUserActionTelemetry = this.sendUserActionTelemetry.bind(this);
    this.onUserAction = this.onUserAction.bind(this);
    this.fetchFlowParams = this.fetchFlowParams.bind(this);
    this.onBlockSelected = this.onBlockSelected.bind(this);
    this.onBlockById = this.onBlockById.bind(this);
    this.onDismiss = this.onDismiss.bind(this);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.state = {
      message: {}
    };

    if (props.document) {
      this.footerPortal = props.document.getElementById("footer-asrouter-container");
    }
  }

  async fetchFlowParams(params = {}) {
    let result = {};
    const {
      fxaEndpoint
    } = this.props;

    if (!fxaEndpoint) {
      const err = "Tried to fetch flow params before fxaEndpoint pref was ready";
      console.error(err); // eslint-disable-line no-console
    }

    try {
      const urlObj = new URL(fxaEndpoint);
      urlObj.pathname = "metrics-flow";
      Object.keys(params).forEach(key => {
        urlObj.searchParams.append(key, params[key]);
      });
      const response = await fetch(urlObj.toString(), {
        credentials: "omit"
      });

      if (response.status === 200) {
        const {
          deviceId,
          flowId,
          flowBeginTime
        } = await response.json();
        result = {
          deviceId,
          flowId,
          flowBeginTime
        };
      } else {
        console.error("Non-200 response", response); // eslint-disable-line no-console
      }
    } catch (error) {
      console.error(error); // eslint-disable-line no-console
    }

    return result;
  }

  sendUserActionTelemetry(extraProps = {}) {
    const {
      message
    } = this.state;
    const eventType = `${message.provider}_user_event`;
    const source = extraProps.id;
    delete extraProps.id;
    _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].sendTelemetry({
      source,
      message_id: message.id,
      action: eventType,
      ...extraProps
    });
  }

  sendImpression(extraProps) {
    if (this.state.message.provider === "preview") {
      return Promise.resolve();
    }

    this.sendUserActionTelemetry({
      event: "IMPRESSION",
      ...extraProps
    });
    return _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].sendMessage({
      type: common_ActorConstants_jsm__WEBPACK_IMPORTED_MODULE_0__["MESSAGE_TYPE_HASH"].IMPRESSION,
      data: this.state.message
    });
  } // If link has a `metric` data attribute send it as part of the `event_context`
  // telemetry field which can have arbitrary values.
  // Used for router messages with links as part of the content.


  sendClick(event) {
    const {
      dataset
    } = event.target;
    const metric = {
      event_context: dataset.metric,
      // Used for the `source` of the event. Needed to differentiate
      // from other snippet or onboarding events that may occur.
      id: "NEWTAB_FOOTER_BAR_CONTENT"
    };
    const {
      entrypoint_name,
      entrypoint_value
    } = dataset; // Assign the snippet referral for the action

    const entrypoint = entrypoint_name ? new URLSearchParams([[entrypoint_name, entrypoint_value]]).toString() : entrypoint_value;
    const action = {
      type: dataset.action,
      data: {
        args: dataset.args,
        ...(entrypoint && {
          entrypoint
        })
      }
    };

    if (action.type) {
      _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].executeAction(action);
    }

    if (!this.state.message.content.do_not_autoblock && !dataset.do_not_autoblock) {
      this.onBlockById(this.state.message.id);
    }

    if (this.state.message.provider !== "preview") {
      this.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        ...metric
      });
    }
  }

  onBlockSelected(options) {
    return this.onBlockById(this.state.message.id, { ...options,
      campaign: this.state.message.campaign
    });
  }

  onBlockById(id, options) {
    return _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].blockById(id, options).then(clearAll => {
      if (clearAll) {
        this.setState({
          message: {}
        });
      }
    });
  }

  onDismiss() {
    this.clearMessage(this.state.message.id);
  } // Blocking a snippet by id blocks the entire campaign
  // so when clearing we use the two values interchangeably


  clearMessage(idOrCampaign) {
    if (idOrCampaign === this.state.message.id || idOrCampaign === this.state.message.campaign) {
      this.setState({
        message: {}
      });
    }
  }

  clearProvider(id) {
    if (this.state.message.provider === id) {
      this.setState({
        message: {}
      });
    }
  }

  onMessageFromParent({
    type,
    data
  }) {
    // These only exists due to onPrefChange events in ASRouter
    switch (type) {
      case "ClearMessages":
        {
          data.forEach(id => this.clearMessage(id));
          break;
        }

      case "ClearProviders":
        {
          data.forEach(id => this.clearProvider(id));
          break;
        }

      case "EnterSnippetsPreviewMode":
        {
          this.props.dispatch({
            type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionTypes"].SNIPPETS_PREVIEW_MODE
          });
          break;
        }
    }
  }

  requestMessage(endpoint) {
    _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].sendMessage({
      type: "NEWTAB_MESSAGE_REQUEST",
      data: {
        endpoint
      }
    }).then(state => this.setState(state));
  }

  componentWillMount() {
    const endpoint = _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].getPreviewEndpoint();

    if (endpoint && endpoint.theme === "dark") {
      global.window.dispatchEvent(new CustomEvent("LightweightTheme:Set", {
        detail: {
          data: content_src_lib_constants__WEBPACK_IMPORTED_MODULE_6__["NEWTAB_DARK_THEME"]
        }
      }));
    }

    if (endpoint && endpoint.dir === "rtl") {
      //Set `dir = rtl` on the HTML
      this.props.document.dir = "rtl";
    }

    _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].addListener(this.onMessageFromParent);
    this.requestMessage(endpoint);
  }

  componentWillUnmount() {
    _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].removeListener(this.onMessageFromParent);
  }

  componentDidUpdate(prevProps) {
    if (prevProps.adminContent && JSON.stringify(prevProps.adminContent) !== JSON.stringify(this.props.adminContent)) {
      this.updateContent();
    }
  }

  updateContent() {
    this.setState({ ...this.props.adminContent
    });
  }

  async getMonitorUrl({
    url,
    flowRequestParams = {}
  }) {
    const flowValues = await this.fetchFlowParams(flowRequestParams); // Note that flowParams are actually added dynamically on the page

    const urlObj = new URL(url);
    ["deviceId", "flowId", "flowBeginTime"].forEach(key => {
      if (key in flowValues) {
        urlObj.searchParams.append(key, flowValues[key]);
      }
    });
    return urlObj.toString();
  }

  async onUserAction(action) {
    switch (action.type) {
      // This needs to be handled locally because its
      case "ENABLE_FIREFOX_MONITOR":
        const url = await this.getMonitorUrl(action.data.args);
        _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].executeAction({
          type: "OPEN_URL",
          data: {
            args: url
          }
        });
        break;

      default:
        _asrouter_utils__WEBPACK_IMPORTED_MODULE_2__["ASRouterUtils"].executeAction(action);
    }
  }

  renderSnippets() {
    const {
      message
    } = this.state;

    if (!_templates_template_manifest__WEBPACK_IMPORTED_MODULE_9__["SnippetsTemplates"][message.template]) {
      return null;
    }

    const SnippetComponent = _templates_template_manifest__WEBPACK_IMPORTED_MODULE_9__["SnippetsTemplates"][message.template];
    const {
      content
    } = this.state.message;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_components_ImpressionsWrapper_ImpressionsWrapper__WEBPACK_IMPORTED_MODULE_4__["ImpressionsWrapper"], {
      id: "NEWTAB_FOOTER_BAR",
      message: this.state.message,
      sendImpression: this.sendImpression,
      shouldSendImpressionOnUpdate: shouldSendImpressionOnUpdate // This helps with testing
      ,
      document: this.props.document
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(fluent_react__WEBPACK_IMPORTED_MODULE_5__["LocalizationProvider"], {
      bundles: Object(_rich_text_strings__WEBPACK_IMPORTED_MODULE_3__["generateBundles"])(content)
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(SnippetComponent, _extends({}, this.state.message, {
      UISurface: "NEWTAB_FOOTER_BAR",
      onBlock: this.onBlockSelected,
      onDismiss: this.onDismiss,
      onAction: this.onUserAction,
      sendClick: this.sendClick,
      sendUserActionTelemetry: this.sendUserActionTelemetry
    }))));
  }

  renderPreviewBanner() {
    if (this.state.message.provider !== "preview") {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: "snippets-preview-banner"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("span", null, "Preview Purposes Only"));
  }

  render() {
    const {
      message
    } = this.state;

    if (!message.id) {
      return null;
    }

    const shouldRenderBelowSearch = TEMPLATES_BELOW_SEARCH.includes(message.template);
    return shouldRenderBelowSearch ?
    /*#__PURE__*/
    // Render special below search snippets in place;
    react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: "below-search-snippet-wrapper"
    }, this.renderSnippets()) :
    /*#__PURE__*/
    // For regular snippets etc. we should render everything in our footer
    // container.
    react_dom__WEBPACK_IMPORTED_MODULE_8___default.a.createPortal( /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_7___default.a.Fragment, null, this.renderPreviewBanner(), this.renderSnippets()), this.footerPortal);
  }

}
ASRouterUISurface.defaultProps = {
  document: global.document
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 11 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "VISIBLE", function() { return VISIBLE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "VISIBILITY_CHANGE_EVENT", function() { return VISIBILITY_CHANGE_EVENT; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ImpressionsWrapper", function() { return ImpressionsWrapper; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      this.props.sendImpression({
        id: this.props.id
      });
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      } // When the page becomes visible, send the impression stats ping if the section isn't collapsed.


      this._onVisibilityChange = () => {
        if (this.props.document.visibilityState === VISIBLE) {
          this.props.sendImpression({
            id: this.props.id
          });
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
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 12 */
/***/ (function(module, exports) {

module.exports = PropTypes;

/***/ }),
/* 13 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "IS_NEWTAB", function() { return IS_NEWTAB; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "NEWTAB_DARK_THEME", function() { return NEWTAB_DARK_THEME; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const IS_NEWTAB = global.document && global.document.documentURI === "about:newtab";
const NEWTAB_DARK_THEME = {
  ntp_background: {
    r: 42,
    g: 42,
    b: 46,
    a: 1
  },
  ntp_text: {
    r: 249,
    g: 249,
    b: 250,
    a: 1
  },
  sidebar: {
    r: 56,
    g: 56,
    b: 61,
    a: 1
  },
  sidebar_text: {
    r: 249,
    g: 249,
    b: 250,
    a: 1
  }
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 14 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 15 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_ConfirmDialog", function() { return _ConfirmDialog; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ConfirmDialog", function() { return ConfirmDialog; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



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

class _ConfirmDialog extends react__WEBPACK_IMPORTED_MODULE_2___default.a.PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_CANCEL
    });
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_CANCEL,
      source: this.props.data.eventSource
    }));
  }

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  }

  _renderModalMessage() {
    const message_body = this.props.data.body_string_id;

    if (!message_body) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("span", null, message_body.map(msg => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("p", {
      key: msg,
      "data-l10n-id": msg
    })));
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("div", {
      className: "confirmation-dialog"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("div", {
      className: "modal-overlay",
      onClick: this._handleCancelBtn,
      role: "presentation"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("div", {
      className: "modal"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("section", {
      className: "modal-message"
    }, this.props.data.icon && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("span", {
      className: `icon icon-spacer icon-${this.props.data.icon}`
    }), this._renderModalMessage()), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("section", {
      className: "actions"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("button", {
      onClick: this._handleCancelBtn,
      "data-l10n-id": this.props.data.cancel_button_string_id
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("button", {
      className: "done",
      onClick: this._handleConfirmBtn,
      "data-l10n-id": this.props.data.confirm_button_string_id
    }))));
  }

}
const ConfirmDialog = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(state => state.Dialog)(_ConfirmDialog);

/***/ }),
/* 16 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "isAllowedCSS", function() { return isAllowedCSS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_DiscoveryStreamBase", function() { return _DiscoveryStreamBase; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DiscoveryStreamBase", function() { return DiscoveryStreamBase; });
/* harmony import */ var content_src_components_DiscoveryStreamComponents_CardGrid_CardGrid__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(17);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_CollectionCardGrid_CollectionCardGrid__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(32);
/* harmony import */ var content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(34);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSMessage_DSMessage__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(39);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSPrivacyModal_DSPrivacyModal__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(40);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSSignup_DSSignup__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(42);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSTextPromo_DSTextPromo__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(43);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_Hero_Hero__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(44);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_Highlights_Highlights__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(46);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_HorizontalRule_HorizontalRule__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(60);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_List_List__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(45);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_Navigation_Navigation__WEBPACK_IMPORTED_MODULE_12__ = __webpack_require__(61);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_13__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_13___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_13__);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_SectionTitle_SectionTitle__WEBPACK_IMPORTED_MODULE_14__ = __webpack_require__(62);
/* harmony import */ var content_src_lib_selectLayoutRender__WEBPACK_IMPORTED_MODULE_15__ = __webpack_require__(63);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_16__ = __webpack_require__(64);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

















const ALLOWED_CSS_URL_PREFIXES = ["chrome://", "resource://", "https://img-getpocket.cdn.mozilla.net/"];
const DUMMY_CSS_SELECTOR = "DUMMY#CSS.SELECTOR";
/**
 * Validate a CSS declaration. The values are assumed to be normalized by CSSOM.
 */

function isAllowedCSS(property, value) {
  // Bug 1454823: INTERNAL properties, e.g., -moz-context-properties, are
  // exposed but their values aren't resulting in getting nothing. Fortunately,
  // we don't care about validating the values of the current set of properties.
  if (value === undefined) {
    return true;
  } // Make sure all urls are of the allowed protocols/prefixes


  const urls = value.match(/url\("[^"]+"\)/g);
  return !urls || urls.every(url => ALLOWED_CSS_URL_PREFIXES.some(prefix => url.slice(5).startsWith(prefix)));
}
class _DiscoveryStreamBase extends react__WEBPACK_IMPORTED_MODULE_13___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onStyleMount = this.onStyleMount.bind(this);
  }

  onStyleMount(style) {
    // Unmounting style gets rid of old styles, so nothing else to do
    if (!style) {
      return;
    }

    const {
      sheet
    } = style;
    const styles = JSON.parse(style.dataset.styles);
    styles.forEach((row, rowIndex) => {
      row.forEach((component, componentIndex) => {
        // Nothing to do without optional styles overrides
        if (!component) {
          return;
        }

        Object.entries(component).forEach(([selectors, declarations]) => {
          // Start with a dummy rule to validate declarations and selectors
          sheet.insertRule(`${DUMMY_CSS_SELECTOR} {}`);
          const [rule] = sheet.cssRules; // Validate declarations and remove any offenders. CSSOM silently
          // discards invalid entries, so here we apply extra restrictions.

          rule.style = declarations;
          [...rule.style].forEach(property => {
            const value = rule.style[property];

            if (!isAllowedCSS(property, value)) {
              console.error(`Bad CSS declaration ${property}: ${value}`); // eslint-disable-line no-console

              rule.style.removeProperty(property);
            }
          }); // Set the actual desired selectors scoped to the component

          const prefix = `.ds-layout > .ds-column:nth-child(${rowIndex + 1}) .ds-column-grid > :nth-child(${componentIndex + 1})`; // NB: Splitting on "," doesn't work with strings with commas, but
          // we're okay with not supporting those selectors

          rule.selectorText = selectors.split(",").map(selector => prefix + ( // Assume :pseudo-classes are for component instead of descendant
          selector[0] === ":" ? "" : " ") + selector).join(","); // CSSOM silently ignores bad selectors, so we'll be noisy instead

          if (rule.selectorText === DUMMY_CSS_SELECTOR) {
            console.error(`Bad CSS selector ${selectors}`); // eslint-disable-line no-console
          }
        });
      });
    });
  }

  renderComponent(component, embedWidth) {
    const ENGAGEMENT_LABEL_ENABLED = this.props.Prefs.values[`discoverystream.engagementLabelEnabled`];

    switch (component.type) {
      case "Highlights":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_Highlights_Highlights__WEBPACK_IMPORTED_MODULE_9__["Highlights"], null);

      case "TopSites":
        let promoAlignment;

        if (component.spocs && component.spocs.positions && component.spocs.positions.length) {
          promoAlignment = component.spocs.positions[0].index === 0 ? "left" : "right";
        }

        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_16__["TopSites"], {
          header: component.header,
          data: component.data,
          promoAlignment: promoAlignment
        });

      case "TextPromo":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSTextPromo_DSTextPromo__WEBPACK_IMPORTED_MODULE_7__["DSTextPromo"], {
          dispatch: this.props.dispatch,
          type: component.type,
          data: component.data
        });

      case "Signup":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSSignup_DSSignup__WEBPACK_IMPORTED_MODULE_6__["DSSignup"], {
          dispatch: this.props.dispatch,
          type: component.type,
          data: component.data
        });

      case "Message":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSMessage_DSMessage__WEBPACK_IMPORTED_MODULE_4__["DSMessage"], {
          title: component.header && component.header.title,
          subtitle: component.header && component.header.subtitle,
          link_text: component.header && component.header.link_text,
          link_url: component.header && component.header.link_url,
          icon: component.header && component.header.icon
        });

      case "SectionTitle":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_SectionTitle_SectionTitle__WEBPACK_IMPORTED_MODULE_14__["SectionTitle"], {
          header: component.header
        });

      case "Navigation":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_Navigation_Navigation__WEBPACK_IMPORTED_MODULE_12__["Navigation"], {
          dispatch: this.props.dispatch,
          links: component.properties.links,
          alignment: component.properties.alignment,
          display_variant: component.properties.display_variant,
          explore_topics: component.properties.explore_topics,
          header: component.header,
          locale: this.props.App.locale,
          privacyNoticeURL: component.properties.privacyNoticeURL
        });

      case "CollectionCardGrid":
        const {
          DiscoveryStream
        } = this.props;
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_CollectionCardGrid_CollectionCardGrid__WEBPACK_IMPORTED_MODULE_1__["CollectionCardGrid"], {
          data: component.data,
          feed: component.feed,
          spocs: DiscoveryStream.spocs,
          placement: component.placement,
          border: component.properties.border,
          type: component.type,
          items: component.properties.items,
          cta_variant: component.cta_variant,
          display_engagement_labels: ENGAGEMENT_LABEL_ENABLED,
          dismissible: this.props.DiscoveryStream.isCollectionDismissible,
          dispatch: this.props.dispatch
        });

      case "CardGrid":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_CardGrid_CardGrid__WEBPACK_IMPORTED_MODULE_0__["CardGrid"], {
          enable_video_playheads: !!component.properties.enable_video_playheads,
          title: component.header && component.header.title,
          display_variant: component.properties.display_variant,
          data: component.data,
          feed: component.feed,
          border: component.properties.border,
          type: component.type,
          dispatch: this.props.dispatch,
          items: component.properties.items,
          cta_variant: component.cta_variant,
          display_engagement_labels: ENGAGEMENT_LABEL_ENABLED
        });

      case "Hero":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_Hero_Hero__WEBPACK_IMPORTED_MODULE_8__["Hero"], {
          subComponentType: embedWidth >= 9 ? `cards` : `list`,
          feed: component.feed,
          title: component.header && component.header.title,
          data: component.data,
          border: component.properties.border,
          type: component.type,
          dispatch: this.props.dispatch,
          items: component.properties.items
        });

      case "HorizontalRule":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_HorizontalRule_HorizontalRule__WEBPACK_IMPORTED_MODULE_10__["HorizontalRule"], null);

      case "List":
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_List_List__WEBPACK_IMPORTED_MODULE_11__["List"], {
          data: component.data,
          feed: component.feed,
          fullWidth: component.properties.full_width,
          hasBorders: component.properties.border === "border",
          hasImages: component.properties.has_images,
          hasNumbers: component.properties.has_numbers,
          items: component.properties.items,
          type: component.type,
          header: component.header
        });

      default:
        return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("div", null, component.type);
    }
  }

  renderStyles(styles) {
    // Use json string as both the key and styles to render so React knows when
    // to unmount and mount a new instance for new styles.
    const json = JSON.stringify(styles);
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("style", {
      key: json,
      "data-styles": json,
      ref: this.onStyleMount
    });
  }

  render() {
    // Select layout render data by adding spocs and position to recommendations
    const {
      layoutRender
    } = Object(content_src_lib_selectLayoutRender__WEBPACK_IMPORTED_MODULE_15__["selectLayoutRender"])({
      state: this.props.DiscoveryStream,
      prefs: this.props.Prefs.values,
      locale: this.props.locale
    });
    const {
      config
    } = this.props.DiscoveryStream; // Allow rendering without extracting special components

    if (!config.collapsible) {
      return this.renderLayout(layoutRender);
    } // Find the first component of a type and remove it from layout


    const extractComponent = type => {
      for (const [rowIndex, row] of Object.entries(layoutRender)) {
        for (const [index, component] of Object.entries(row.components)) {
          if (component.type === type) {
            // Remove the row if it was the only component or the single item
            if (row.components.length === 1) {
              layoutRender.splice(rowIndex, 1);
            } else {
              row.components.splice(index, 1);
            }

            return component;
          }
        }
      }

      return null;
    }; // Get "topstories" Section state for default values


    const topStories = this.props.Sections.find(s => s.id === "topstories");

    if (!topStories) {
      return null;
    } // Extract TopSites to render before the rest and Message to use for header


    const topSites = extractComponent("TopSites");
    const sponsoredCollection = extractComponent("CollectionCardGrid");
    const message = extractComponent("Message") || {
      header: {
        link_text: topStories.learnMore.link.message,
        link_url: topStories.learnMore.link.href,
        title: topStories.title
      }
    }; // Render a DS-style TopSites then the rest if any in a collapsible section

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_13___default.a.Fragment, null, this.props.DiscoveryStream.isPrivacyInfoModalVisible && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSPrivacyModal_DSPrivacyModal__WEBPACK_IMPORTED_MODULE_5__["DSPrivacyModal"], {
      dispatch: this.props.dispatch
    }), topSites && this.renderLayout([{
      width: 12,
      components: [topSites]
    }]), sponsoredCollection && this.renderLayout([{
      width: 12,
      components: [sponsoredCollection]
    }]), !!layoutRender.length && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement(content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__["CollapsibleSection"], {
      className: "ds-layout",
      collapsed: topStories.pref.collapsed,
      dispatch: this.props.dispatch,
      icon: topStories.icon,
      id: topStories.id,
      isFixed: true,
      learnMore: {
        link: {
          href: message.header.link_url,
          message: message.header.link_text
        }
      },
      privacyNoticeURL: topStories.privacyNoticeURL,
      showPrefName: topStories.pref.feed,
      title: message.header.title,
      eventSource: "CARDGRID"
    }, this.renderLayout(layoutRender)), this.renderLayout([{
      width: 12,
      components: [{
        type: "Highlights"
      }]
    }]));
  }

  renderLayout(layoutRender) {
    const styles = [];
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("div", {
      className: "discovery-stream ds-layout"
    }, layoutRender.map((row, rowIndex) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("div", {
      key: `row-${rowIndex}`,
      className: `ds-column ds-column-${row.width}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("div", {
      className: "ds-column-grid"
    }, row.components.map((component, componentIndex) => {
      if (!component) {
        return null;
      }

      styles[rowIndex] = [...(styles[rowIndex] || []), component.styles];
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_13___default.a.createElement("div", {
        key: `component-${componentIndex}`
      }, this.renderComponent(component, row.width));
    })))), this.renderStyles(styles));
  }

}
const DiscoveryStreamBase = Object(react_redux__WEBPACK_IMPORTED_MODULE_3__["connect"])(state => ({
  DiscoveryStream: state.DiscoveryStream,
  Prefs: state.Prefs,
  Sections: state.Sections,
  document: global.document,
  App: state.App
}))(_DiscoveryStreamBase);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 17 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CardGrid", function() { return CardGrid; });
/* harmony import */ var _DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(18);
/* harmony import */ var _DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(31);
/* harmony import */ var _FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(30);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




class CardGrid extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  renderCards() {
    const recs = this.props.data.recommendations.slice(0, this.props.items);
    const cards = [];

    for (let index = 0; index < this.props.items; index++) {
      const rec = recs[index];
      cards.push(!rec || rec.placeholder ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__["PlaceholderDSCard"], {
        key: `dscard-${index}`
      }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__["DSCard"], {
        key: `dscard-${rec.id}`,
        pos: rec.pos,
        flightId: rec.flight_id,
        image_src: rec.image_src,
        raw_image_src: rec.raw_image_src,
        title: rec.title,
        excerpt: rec.excerpt,
        url: rec.url,
        id: rec.id,
        shim: rec.shim,
        type: this.props.type,
        context: rec.context,
        sponsor: rec.sponsor,
        sponsored_by_override: rec.sponsored_by_override,
        dispatch: this.props.dispatch,
        source: rec.domain,
        pocket_id: rec.pocket_id,
        context_type: rec.context_type,
        bookmarkGuid: rec.bookmarkGuid,
        engagement: rec.engagement,
        display_engagement_labels: this.props.display_engagement_labels,
        cta: rec.cta,
        cta_variant: this.props.cta_variant,
        is_video: this.props.enable_video_playheads && rec.is_video,
        is_collection: this.props.is_collection
      }));
    } // Used for CSS overrides to default styling (eg: "hero")


    const variantClass = this.props.display_variant ? `ds-card-grid-${this.props.display_variant}` : ``;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: `ds-card-grid ds-card-grid-${this.props.border} ${variantClass}`
    }, cards);
  }

  render() {
    const {
      data
    } = this.props; // Handle a render before feed has been fetched by displaying nothing

    if (!data) {
      return null;
    } // Handle the case where a user has dismissed all recommendations


    const isEmpty = data.recommendations.length === 0;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, this.props.title && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "ds-header"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "title"
    }, this.props.title), this.props.context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
      message: this.props.context
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "ds-context"
    }))), isEmpty ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "ds-card-grid empty"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_1__["DSEmptyState"], {
      status: data.status,
      dispatch: this.props.dispatch,
      feed: this.props.feed
    })) : this.renderCards());
  }

}
CardGrid.defaultProps = {
  border: `border`,
  items: 4,
  // Number of stories to display
  enable_video_playheads: false
};

/***/ }),
/* 18 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DefaultMeta", function() { return DefaultMeta; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CTAButtonMeta", function() { return CTAButtonMeta; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_DSCard", function() { return _DSCard; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSCard", function() { return DSCard; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PlaceholderDSCard", function() { return PlaceholderDSCard; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var _DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(19);
/* harmony import */ var _DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(20);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(25);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(26);
/* harmony import */ var _DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(27);
/* harmony import */ var _FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(30);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_8___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_8__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */








 // Default Meta that displays CTA as link if cta_variant in layout is set as "link"

const DefaultMeta = ({
  display_engagement_labels,
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
  engagement,
  cta_variant,
  sponsor,
  sponsored_by_override
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
  className: "meta"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
  className: "info-wrap"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("p", {
  className: "source clamp"
}, source), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("header", {
  className: "title clamp"
}, title), excerpt && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("p", {
  className: "excerpt clamp"
}, excerpt), cta_variant === "link" && cta && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
  role: "link",
  className: "cta-link icon icon-arrow",
  tabIndex: "0"
}, cta)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_6__["DSContextFooter"], {
  context_type: context_type,
  context: context,
  sponsor: sponsor,
  sponsored_by_override: sponsored_by_override,
  display_engagement_labels: display_engagement_labels,
  engagement: engagement
}));
const CTAButtonMeta = ({
  display_engagement_labels,
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
  engagement,
  sponsor,
  sponsored_by_override
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
  className: "meta"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
  className: "info-wrap"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("p", {
  className: "source clamp"
}, context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_7__["FluentOrText"], {
  message: {
    id: `newtab-label-sponsored`,
    values: {
      sponsorOrSource: sponsor ? sponsor : source
    }
  }
}), !context && (sponsor ? sponsor : source)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("header", {
  className: "title clamp"
}, title), excerpt && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("p", {
  className: "excerpt clamp"
}, excerpt)), context && cta && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("button", {
  className: "button cta-button"
}, cta), !context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_6__["DSContextFooter"], {
  context_type: context_type,
  context: context,
  sponsor: sponsor,
  sponsored_by_override: sponsored_by_override,
  display_engagement_labels: display_engagement_labels,
  engagement: engagement
}));
class _DSCard extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);

    this.setPlaceholderRef = element => {
      this.placeholderElement = element;
    };

    this.state = {
      isSeen: false
    }; // If this is for the about:home startup cache, then we always want
    // to render the DSCard, regardless of whether or not its been seen.

    if (props.App.isForStartupCache) {
      this.state.isSeen = true;
    } // We want to choose the optimal thumbnail for the underlying DSImage, but
    // want to do it in a performant way. The breakpoints used in the
    // CSS of the page are, unfortuntely, not easy to retrieve without
    // causing a style flush. To avoid that, we hardcode them here.
    //
    // The values chosen here were the dimensions of the card thumbnails as
    // computed by getBoundingClientRect() for each type of viewport width
    // across both high-density and normal-density displays.


    this.dsImageSizes = [{
      mediaMatcher: "(min-width: 1122px)",
      width: 296,
      height: 148
    }, {
      mediaMatcher: "(min-width: 866px)",
      width: 218,
      height: 109
    }, {
      mediaMatcher: "(max-width: 610px)",
      width: 202,
      height: 101
    }];
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "CLICK",
        source: this.props.is_video ? "CARDGRID_VIDEO" : this.props.type.toUpperCase(),
        action_position: this.props.pos,
        value: {
          card_type: this.props.flightId ? "spoc" : "organic"
        }
      }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
        source: this.props.is_video ? "CARDGRID_VIDEO" : this.props.type.toUpperCase(),
        click: 0,
        tiles: [{
          id: this.props.id,
          pos: this.props.pos,
          ...(this.props.shim && this.props.shim.click ? {
            shim: this.props.shim.click
          } : {})
        }]
      }));
    }
  }

  onSeen(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.placeholderElement) {
          this.observer.unobserve(this.placeholderElement);
        } // Stop observing since element has been seen


        this.setState({
          isSeen: true
        });
      }
    }
  }

  onIdleCallback() {
    if (!this.state.isSeen) {
      if (this.observer && this.placeholderElement) {
        this.observer.unobserve(this.placeholderElement);
      }

      this.setState({
        isSeen: true
      });
    }
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(this.onIdleCallback.bind(this));

    if (this.placeholderElement) {
      this.observer = new IntersectionObserver(this.onSeen.bind(this));
      this.observer.observe(this.placeholderElement);
    }
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer && this.placeholderElement) {
      this.observer.unobserve(this.placeholderElement);
    }

    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    if (this.props.placeholder || !this.state.isSeen) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
        className: "ds-card placeholder",
        ref: this.setPlaceholderRef
      });
    }

    const isButtonCTA = this.props.cta_variant === "button";
    const baseClass = `ds-card ${this.props.is_video ? `video-card` : ``}`;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: baseClass
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_5__["SafeAnchor"], {
      className: "ds-card-link",
      dispatch: this.props.dispatch,
      onLinkClick: !this.props.placeholder ? this.onLinkClick : undefined,
      url: this.props.url
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "img-wrapper"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_1__["DSImage"], {
      extraClassNames: "img",
      source: this.props.image_src,
      rawSource: this.props.raw_image_src,
      sizes: this.dsImageSizes
    }), this.props.is_video && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "playhead"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", null, "Video Content"))), isButtonCTA ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(CTAButtonMeta, {
      display_engagement_labels: this.props.display_engagement_labels,
      source: this.props.source,
      title: this.props.title,
      excerpt: this.props.excerpt,
      context: this.props.context,
      context_type: this.props.context_type,
      engagement: this.props.engagement,
      cta: this.props.cta,
      sponsor: this.props.sponsor,
      sponsored_by_override: this.props.sponsored_by_override
    }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(DefaultMeta, {
      display_engagement_labels: this.props.display_engagement_labels,
      source: this.props.source,
      title: this.props.title,
      excerpt: this.props.excerpt,
      context: this.props.context,
      engagement: this.props.engagement,
      context_type: this.props.context_type,
      cta: this.props.cta,
      cta_variant: this.props.cta_variant,
      sponsor: this.props.sponsor,
      sponsored_by_override: this.props.sponsored_by_override
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__["ImpressionStats"], {
      flightId: this.props.flightId,
      rows: [{
        id: this.props.id,
        pos: this.props.pos,
        ...(this.props.shim && this.props.shim.impression ? {
          shim: this.props.shim.impression
        } : {})
      }],
      dispatch: this.props.dispatch,
      source: this.props.is_video ? "CARDGRID_VIDEO" : this.props.type
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_2__["DSLinkMenu"], {
      id: this.props.id,
      index: this.props.pos,
      dispatch: this.props.dispatch,
      url: this.props.url,
      title: this.props.title,
      source: this.props.source,
      type: this.props.type,
      pocket_id: this.props.pocket_id,
      shim: this.props.shim,
      bookmarkGuid: this.props.bookmarkGuid,
      flightId: !this.props.is_collection ? this.props.flightId : undefined,
      showPrivacyInfo: !!this.props.flightId
    }));
  }

}
_DSCard.defaultProps = {
  windowObj: window // Added to support unit tests

};
const DSCard = Object(react_redux__WEBPACK_IMPORTED_MODULE_8__["connect"])(state => ({
  App: state.App
}))(_DSCard);
const PlaceholderDSCard = props => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(DSCard, {
  placeholder: true
});

/***/ }),
/* 19 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSImage", function() { return DSImage; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class DSImage extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onOptimizedImageError = this.onOptimizedImageError.bind(this);
    this.onNonOptimizedImageError = this.onNonOptimizedImageError.bind(this);
    this.onLoad = this.onLoad.bind(this);
    this.state = {
      isLoaded: false,
      optimizedImageFailed: false,
      useTransition: false
    };
  }

  onIdleCallback() {
    if (!this.state.isLoaded) {
      this.setState({
        useTransition: true
      });
    }
  }

  reformatImageURL(url, width, height) {
    // Change the image URL to request a size tailored for the parent container width
    // Also: force JPEG, quality 60, no upscaling, no EXIF data
    // Uses Thumbor: https://thumbor.readthedocs.io/en/latest/usage.html
    return `https://img-getpocket.cdn.mozilla.net/${width}x${height}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(url)}`;
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(this.onIdleCallback.bind(this));
  }

  componentWillUnmount() {
    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    let classNames = `ds-image
      ${this.props.extraClassNames ? ` ${this.props.extraClassNames}` : ``}
      ${this.state && this.state.useTransition ? ` use-transition` : ``}
      ${this.state && this.state.isLoaded ? ` loaded` : ``}
    `;
    let img;

    if (this.state) {
      if (this.props.optimize && this.props.rawSource && !this.state.optimizedImageFailed) {
        let baseSource = this.props.rawSource;
        let sizeRules = [];
        let srcSetRules = [];

        for (let rule of this.props.sizes) {
          let {
            mediaMatcher,
            width,
            height
          } = rule;
          let sizeRule = `${mediaMatcher} ${width}px`;
          sizeRules.push(sizeRule);
          let srcSetRule = `${this.reformatImageURL(baseSource, width, height)} ${width}w`;
          let srcSetRule2x = `${this.reformatImageURL(baseSource, width * 2, height * 2)} ${width * 2}w`;
          srcSetRules.push(srcSetRule);
          srcSetRules.push(srcSetRule2x);
        }

        if (this.props.sizes.length) {
          // We have to supply a fallback in the very unlikely event that none of
          // the media queries match. The smallest dimension was chosen arbitrarily.
          sizeRules.push(`${this.props.sizes[this.props.sizes.length - 1].width}px`);
        }

        img = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("img", {
          loading: "lazy",
          alt: this.props.alt_text,
          crossOrigin: "anonymous",
          onLoad: this.onLoad,
          onError: this.onOptimizedImageError,
          sizes: sizeRules.join(","),
          src: baseSource,
          srcSet: srcSetRules.join(",")
        });
      } else if (!this.state.nonOptimizedImageFailed) {
        img = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("img", {
          loading: "lazy",
          alt: this.props.alt_text,
          crossOrigin: "anonymous",
          onLoad: this.onLoad,
          onError: this.onNonOptimizedImageError,
          src: this.props.source
        });
      } else {
        // Remove the img element if both sources fail. Render a placeholder instead.
        img = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
          className: "broken-image"
        });
      }
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("picture", {
      className: classNames
    }, img);
  }

  onOptimizedImageError() {
    // This will trigger a re-render and the unoptimized 450px image will be used as a fallback
    this.setState({
      optimizedImageFailed: true
    });
  }

  onNonOptimizedImageError() {
    this.setState({
      nonOptimizedImageFailed: true
    });
  }

  onLoad() {
    this.setState({
      isLoaded: true
    });
  }

}
DSImage.defaultProps = {
  source: null,
  // The current source style from Pocket API (always 450px)
  rawSource: null,
  // Unadulterated image URL to filter through Thumbor
  extraClassNames: null,
  // Additional classnames to append to component
  optimize: true,
  // Measure parent container to request exact sizes
  alt_text: null,
  windowObj: window,
  // Added to support unit tests
  sizes: []
};

/***/ }),
/* 20 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSLinkMenu", function() { return DSLinkMenu; });
/* harmony import */ var content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(21);
/* harmony import */ var content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(24);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class DSLinkMenu extends react__WEBPACK_IMPORTED_MODULE_2___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
    this.contextMenuButtonRef = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createRef();
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
      dsLinkMenuHostDiv.parentElement.classList.remove("active", "last-item");
    }
  }

  nextAnimationFrame() {
    return new Promise(resolve => this.props.windowObj.requestAnimationFrame(resolve));
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement; // Wait for next frame before computing scrollMaxX to allow fluent menu strings to be visible

    await this.nextAnimationFrame();

    if (this.props.windowObj.scrollMaxX > 0) {
      dsLinkMenuHostDiv.parentElement.classList.add("last-item");
    }

    dsLinkMenuHostDiv.parentElement.classList.add("active");
  }

  render() {
    const {
      index,
      dispatch
    } = this.props;
    const TOP_STORIES_CONTEXT_MENU_OPTIONS = ["CheckBookmarkOrArchive", "CheckSavedToPocket", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", ...(this.props.showPrivacyInfo ? ["ShowPrivacyInfo"] : [])];
    const type = this.props.type || "DISCOVERY_STREAM";
    const title = this.props.title || this.props.source;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement(content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_1__["ContextMenuButton"], {
      refFunction: this.contextMenuButtonRef,
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuUpdate
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement(content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_0__["LinkMenu"], {
      dispatch: dispatch,
      index: index,
      source: type.toUpperCase(),
      onShow: this.onMenuShow,
      options: TOP_STORIES_CONTEXT_MENU_OPTIONS,
      shouldSendImpressionStats: true,
      site: {
        referrer: "https://getpocket.com/recommendations",
        title: this.props.title,
        type: this.props.type,
        url: this.props.url,
        guid: this.props.id,
        pocket_id: this.props.pocket_id,
        shim: this.props.shim,
        bookmarkGuid: this.props.bookmarkGuid,
        flight_id: this.props.flightId
      }
    })));
  }

}
DSLinkMenu.defaultProps = {
  windowObj: window // Added to support unit tests

};

/***/ }),
/* 21 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_LinkMenu", function() { return _LinkMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "LinkMenu", function() { return LinkMenu; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(22);
/* harmony import */ var content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(23);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];
class _LinkMenu extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  getOptions() {
    const {
      props
    } = this;
    const {
      site,
      index,
      source,
      isPrivateBrowsingEnabled,
      siteInfo,
      platform
    } = props; // Handle special case of default site

    const propOptions = site.isDefault && !site.searchTopSite && !site.sponsored_position ? DEFAULT_SITE_MENU_OPTIONS : props.options;
    const options = propOptions.map(o => content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_3__["LinkMenuOptions"][o](site, index, source, isPrivateBrowsingEnabled, siteInfo, platform)).map(option => {
      const {
        action,
        impression,
        id,
        type,
        userEvent
      } = option;

      if (!type && id) {
        option.onClick = (event = {}) => {
          const {
            ctrlKey,
            metaKey,
            shiftKey,
            button
          } = event; // Only send along event info if there's something non-default to send

          if (ctrlKey || metaKey || shiftKey || button === 1) {
            action.data = Object.assign({
              event: {
                ctrlKey,
                metaKey,
                shiftKey,
                button
              }
            }, action.data);
          }

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
    }); // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.

    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_2__["ContextMenu"], {
      onUpdate: this.props.onUpdate,
      onShow: this.props.onShow,
      options: this.getOptions(),
      keyboardAccess: this.props.keyboardAccess
    });
  }

}

const getState = state => ({
  isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled,
  platform: state.Prefs.values.platform
});

const LinkMenu = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(getState)(_LinkMenu);

/***/ }),
/* 22 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ContextMenu", function() { return ContextMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_ContextMenuItem", function() { return _ContextMenuItem; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ContextMenuItem", function() { return ContextMenuItem; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class ContextMenu extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.hideContext = this.hideContext.bind(this);
    this.onShow = this.onShow.bind(this);
    this.onClick = this.onClick.bind(this);
  }

  hideContext() {
    this.props.onUpdate(false);
  }

  onShow() {
    if (this.props.onShow) {
      this.props.onShow();
    }
  }

  componentDidMount() {
    this.onShow();
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
    // Disabling focus on the menu span allows the first tab to focus on the first menu item instead of the wrapper.
    return (
      /*#__PURE__*/
      // eslint-disable-next-line jsx-a11y/interactive-supports-focus
      react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
        className: "context-menu"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("ul", {
        role: "menu",
        onClick: this.onClick,
        onKeyDown: this.onClick,
        className: "context-menu-list"
      }, this.props.options.map((option, i) => option.type === "separator" ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("li", {
        key: i,
        className: "separator",
        role: "separator"
      }) : option.type !== "empty" && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(ContextMenuItem, {
        key: i,
        option: option,
        hideContext: this.hideContext,
        keyboardAccess: this.props.keyboardAccess
      }))))
    );
  }

}
class _ContextMenuItem extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onKeyUp = this.onKeyUp.bind(this);
    this.focusFirst = this.focusFirst.bind(this);
  }

  onClick(event) {
    this.props.hideContext();
    this.props.option.onClick(event);
  } // Focus the first menu item if the menu was accessed via the keyboard.


  focusFirst(button) {
    if (this.props.keyboardAccess && button) {
      button.focus();
    }
  } // This selects the correct node based on the key pressed


  focusSibling(target, key) {
    const parent = target.parentNode;
    const closestSiblingSelector = key === "ArrowUp" ? "previousSibling" : "nextSibling";

    if (!parent[closestSiblingSelector]) {
      return;
    }

    if (parent[closestSiblingSelector].firstElementChild) {
      parent[closestSiblingSelector].firstElementChild.focus();
    } else {
      parent[closestSiblingSelector][closestSiblingSelector].firstElementChild.focus();
    }
  }

  onKeyDown(event) {
    const {
      option
    } = this.props;

    switch (event.key) {
      case "Tab":
        // tab goes down in context menu, shift + tab goes up in context menu
        // if we're on the last item, one more tab will close the context menu
        // similarly, if we're on the first item, one more shift + tab will close it
        if (event.shiftKey && option.first || !event.shiftKey && option.last) {
          this.props.hideContext();
        }

        break;

      case "ArrowUp":
      case "ArrowDown":
        event.preventDefault();
        this.focusSibling(event.target, event.key);
        break;

      case "Enter":
      case " ":
        event.preventDefault();
        this.props.hideContext();
        option.onClick();
        break;

      case "Escape":
        this.props.hideContext();
        break;
    }
  } // Prevents the default behavior of spacebar
  // scrolling the page & auto-triggering buttons.


  onKeyUp(event) {
    if (event.key === " ") {
      event.preventDefault();
    }
  }

  render() {
    const {
      option
    } = this.props;
    const {
      newNewtabExperienceEnabled
    } = this.props.Prefs.values.featureConfig;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("li", {
      role: "presentation",
      className: "context-menu-item"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", {
      className: option.disabled ? "disabled" : "",
      role: "menuitem",
      onClick: this.onClick,
      onKeyDown: this.onKeyDown,
      onKeyUp: this.onKeyUp,
      ref: option.first ? this.focusFirst : null
    }, !newNewtabExperienceEnabled && option.icon && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      className: `icon icon-spacer icon-${option.icon}`
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      "data-l10n-id": option.string_id || option.id
    })));
  }

}
const ContextMenuItem = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(state => ({
  Prefs: state.Prefs
}))(_ContextMenuItem);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 23 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "LinkMenuOptions", function() { return LinkMenuOptions; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const _OpenInPrivateWindow = site => ({
  id: "newtab-menu-open-new-private-window",
  icon: "new-window-private",
  action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
    type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_PRIVATE_WINDOW,
    data: {
      url: site.url,
      referrer: site.referrer
    }
  }),
  userEvent: "OPEN_PRIVATE_WINDOW"
});
/**
 * List of functions that return items that can be included as menu options in a
 * LinkMenu. All functions take the site as the first parameter, and optionally
 * the index of the site.
 */


const LinkMenuOptions = {
  Separator: () => ({
    type: "separator"
  }),
  EmptyItem: () => ({
    type: "empty"
  }),
  ShowPrivacyInfo: site => ({
    id: "newtab-menu-show-privacy-info",
    icon: "info",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SHOW_PRIVACY_INFO
    },
    userEvent: "SHOW_PRIVACY_INFO"
  }),
  AboutSponsored: site => ({
    id: "newtab-menu-show-privacy-info",
    icon: "info",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].ABOUT_SPONSORED_TOP_SITES
    }),
    userEvent: "TOPSITE_SPONSOR_INFO"
  }),
  RemoveBookmark: site => ({
    id: "newtab-menu-remove-bookmark",
    icon: "bookmark-added",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "newtab-menu-bookmark",
    icon: "bookmark-hollow",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].BOOKMARK_URL,
      data: {
        url: site.url,
        title: site.title,
        type: site.type
      }
    }),
    userEvent: "BOOKMARK_ADD"
  }),
  OpenInNewWindow: site => ({
    id: "newtab-menu-open-new-window",
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
  // This blocks the url for regular stories,
  // but also sends a message to DiscoveryStream with flight_id.
  // If DiscoveryStream sees this message for a flight_id
  // it also blocks it on the flight_id.
  BlockUrl: (site, index, eventSource) => {
    return LinkMenuOptions.BlockUrls([site], index, eventSource);
  },
  // Same as BlockUrl, cept can work on an array of sites.
  BlockUrls: (tiles, pos, eventSource) => ({
    id: "newtab-menu-dismiss",
    icon: "dismiss",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].BLOCK_URL,
      data: tiles.map(site => ({
        url: site.original_url || site.open_url || site.url,
        // pocket_id is only for pocket stories being in highlights, and then dismissed.
        pocket_id: site.pocket_id,
        ...(site.flight_id ? {
          flight_id: site.flight_id
        } : {})
      }))
    }),
    impression: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
      source: eventSource,
      block: 0,
      tiles: tiles.map((site, index) => ({
        id: site.guid,
        pos: pos + index,
        ...(site.shim && site.shim.delete ? {
          shim: site.shim.delete
        } : {})
      }))
    }),
    userEvent: "BLOCK"
  }),
  // This is an option for web extentions which will result in remove items from
  // memory and notify the web extenion, rather than using the built-in block list.
  WebExtDismiss: (site, index, eventSource) => ({
    id: "menu_action_webext_dismiss",
    string_id: "newtab-menu-dismiss",
    icon: "dismiss",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].WebExtEvent(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].WEBEXT_DISMISS, {
      source: eventSource,
      url: site.url,
      action_position: index
    })
  }),
  DeleteUrl: (site, index, eventSource, isEnabled, siteInfo) => ({
    id: "newtab-menu-delete-history",
    icon: "delete",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DIALOG_OPEN,
      data: {
        onConfirm: [common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
          type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_HISTORY_URL,
          data: {
            url: site.url,
            pocket_id: site.pocket_id,
            forceBlock: site.bookmarkGuid
          }
        }), common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(Object.assign({
          event: "DELETE",
          source: eventSource,
          action_position: index
        }, siteInfo))],
        eventSource,
        body_string_id: ["newtab-confirm-delete-history-p1", "newtab-confirm-delete-history-p2"],
        confirm_button_string_id: "newtab-topsites-delete-history-button",
        cancel_button_string_id: "newtab-topsites-cancel-button",
        icon: "modal-delete"
      }
    },
    userEvent: "DIALOG_OPEN"
  }),
  ShowFile: site => ({
    id: "newtab-menu-show-file",
    icon: "search",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SHOW_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  OpenFile: site => ({
    id: "newtab-menu-open-file",
    icon: "open-file",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  CopyDownloadLink: site => ({
    id: "newtab-menu-copy-download-link",
    icon: "copy",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].COPY_DOWNLOAD_LINK,
      data: {
        url: site.url
      }
    })
  }),
  GoToDownloadPage: site => ({
    id: "newtab-menu-go-to-download-page",
    icon: "download",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
      data: {
        url: site.referrer
      }
    }),
    disabled: !site.referrer
  }),
  RemoveDownload: site => ({
    id: "newtab-menu-remove-download",
    icon: "delete",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].REMOVE_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  PinTopSite: (site, index) => ({
    id: "newtab-menu-pin",
    icon: "pin",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_PIN,
      data: {
        site,
        index
      }
    }),
    userEvent: "PIN"
  }),
  UnpinTopSite: site => ({
    id: "newtab-menu-unpin",
    icon: "unpin",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_UNPIN,
      data: {
        site: {
          url: site.url
        }
      }
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: (site, index, eventSource) => ({
    id: "newtab-menu-save-to-pocket",
    icon: "pocket-save",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_TO_POCKET,
      data: {
        site: {
          url: site.url,
          title: site.title
        }
      }
    }),
    impression: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
      source: eventSource,
      pocket: 0,
      tiles: [{
        id: site.guid,
        pos: index,
        ...(site.shim && site.shim.save ? {
          shim: site.shim.save
        } : {})
      }]
    }),
    userEvent: "SAVE_TO_POCKET"
  }),
  DeleteFromPocket: site => ({
    id: "newtab-menu-delete-pocket",
    icon: "pocket-delete",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DELETE_FROM_POCKET,
      data: {
        pocket_id: site.pocket_id
      }
    }),
    userEvent: "DELETE_FROM_POCKET"
  }),
  ArchiveFromPocket: site => ({
    id: "newtab-menu-archive-pocket",
    icon: "pocket-archive",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].ARCHIVE_FROM_POCKET,
      data: {
        pocket_id: site.pocket_id
      }
    }),
    userEvent: "ARCHIVE_FROM_POCKET"
  }),
  EditTopSite: (site, index) => ({
    id: "newtab-menu-edit-topsites",
    icon: "edit",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT,
      data: {
        index
      }
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
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ContextMenuButton", function() { return ContextMenuButton; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class ContextMenuButton extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      showContextMenu: false,
      contextMenuKeyboard: false
    };
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onUpdate = this.onUpdate.bind(this);
  }

  openContextMenu(isKeyBoard, event) {
    if (this.props.onUpdate) {
      this.props.onUpdate(true);
    }

    this.setState({
      showContextMenu: true,
      contextMenuKeyboard: isKeyBoard
    });
  }

  onClick(event) {
    event.preventDefault();
    this.openContextMenu(false, event);
  }

  onKeyDown(event) {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      this.openContextMenu(true, event);
    }
  }

  onUpdate(showContextMenu) {
    if (this.props.onUpdate) {
      this.props.onUpdate(showContextMenu);
    }

    this.setState({
      showContextMenu
    });
  }

  render() {
    const {
      tooltipArgs,
      tooltip,
      children,
      refFunction
    } = this.props;
    const {
      showContextMenu,
      contextMenuKeyboard
    } = this.state;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_0___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", {
      "aria-haspopup": "true",
      "data-l10n-id": tooltip,
      "data-l10n-args": tooltipArgs ? JSON.stringify(tooltipArgs) : null,
      className: "context-menu-button icon",
      onKeyDown: this.onKeyDown,
      onClick: this.onClick,
      ref: refFunction
    }), showContextMenu ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.cloneElement(children, {
      keyboardAccess: contextMenuKeyboard,
      onUpdate: this.onUpdate
    }) : null);
  }

}

/***/ }),
/* 25 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "INTERSECTION_RATIO", function() { return INTERSECTION_RATIO; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ImpressionStats", function() { return ImpressionStats; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange"; // Per analytical requirement, we set the minimal intersection ratio to
// 0.5, and an impression is identified when the wrapped item has at least
// 50% visibility.
//
// This constant is exported for unit test

const INTERSECTION_RATIO = 0.5;
/**
 * Impression wrapper for Discovery Stream related React components.
 *
 * It makses use of the Intersection Observer API to detect the visibility,
 * and relies on page visibility to ensure the impression is reported
 * only when the component is visible on the page.
 *
 * Note:
 *   * This wrapper used to be used either at the individual card level,
 *     or by the card container components.
 *     It is now only used for individual card level.
 *   * Each impression will be sent only once as soon as the desired
 *     visibility is detected
 *   * Batching is not yet implemented, hence it might send multiple
 *     impression pings separately
 */

class ImpressionStats extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  // This checks if the given cards are the same as those in the last impression ping.
  // If so, it should not send the same impression ping again.
  _needsImpressionStats(cards) {
    if (!this.impressionCardGuids || this.impressionCardGuids.length !== cards.length) {
      return true;
    }

    for (let i = 0; i < cards.length; i++) {
      if (cards[i].id !== this.impressionCardGuids[i]) {
        return true;
      }
    }

    return false;
  }

  _dispatchImpressionStats() {
    const {
      props
    } = this;
    const cards = props.rows;

    if (this.props.flightId) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: {
          flightId: this.props.flightId
        }
      }));
    }

    if (this._needsImpressionStats(cards)) {
      props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].DiscoveryStreamImpressionStats({
        source: props.source.toUpperCase(),
        tiles: cards.map(link => ({
          id: link.id,
          pos: link.pos,
          ...(link.shim ? {
            shim: link.shim
          } : {})
        }))
      }));
      this.impressionCardGuids = cards.map(link => link.id);
    }
  } // This checks if the given cards are the same as those in the last loaded content ping.
  // If so, it should not send the same loaded content ping again.


  _needsLoadedContent(cards) {
    if (!this.loadedContentGuids || this.loadedContentGuids.length !== cards.length) {
      return true;
    }

    for (let i = 0; i < cards.length; i++) {
      if (cards[i].id !== this.loadedContentGuids[i]) {
        return true;
      }
    }

    return false;
  }

  _dispatchLoadedContent() {
    const {
      props
    } = this;
    const cards = props.rows;

    if (this._needsLoadedContent(cards)) {
      props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].DiscoveryStreamLoadedContent({
        source: props.source.toUpperCase(),
        tiles: cards.map(link => ({
          id: link.id,
          pos: link.pos
        }))
      }));
      this.loadedContentGuids = cards.map(link => link.id);
    }
  }

  setImpressionObserverOrAddListener() {
    const {
      props
    } = this;

    if (!props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
      // Send the loaded content ping once the page is visible.
      this._dispatchLoadedContent();

      this.setImpressionObserver();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      this._onVisibilityChange = () => {
        if (props.document.visibilityState === VISIBLE) {
          // Send the loaded content ping once the page is visible.
          this._dispatchLoadedContent();

          this.setImpressionObserver();
          props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };

      props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }
  /**
   * Set an impression observer for the wrapped component. It makes use of
   * the Intersection Observer API to detect if the wrapped component is
   * visible with a desired ratio, and only sends impression if that's the case.
   *
   * See more details about Intersection Observer API at:
   * https://developer.mozilla.org/en-US/docs/Web/API/Intersection_Observer_API
   */


  setImpressionObserver() {
    const {
      props
    } = this;

    if (!props.rows.length) {
      return;
    }

    this._handleIntersect = entries => {
      if (entries.some(entry => entry.isIntersecting && entry.intersectionRatio >= INTERSECTION_RATIO)) {
        this._dispatchImpressionStats();

        this.impressionObserver.unobserve(this.refs.impression);
      }
    };

    const options = {
      threshold: INTERSECTION_RATIO
    };
    this.impressionObserver = new props.IntersectionObserver(this._handleIntersect, options);
    this.impressionObserver.observe(this.refs.impression);
  }

  componentDidMount() {
    if (this.props.rows.length) {
      this.setImpressionObserverOrAddListener();
    }
  }

  componentWillUnmount() {
    if (this._handleIntersect && this.impressionObserver) {
      this.impressionObserver.unobserve(this.refs.impression);
    }

    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      ref: "impression",
      className: "impression-observer"
    }, this.props.children);
  }

}
ImpressionStats.defaultProps = {
  IntersectionObserver: global.IntersectionObserver,
  document: global.document,
  rows: [],
  source: ""
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 26 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SafeAnchor", function() { return SafeAnchor; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class SafeAnchor extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick(event) {
    // Use dispatch instead of normal link click behavior to include referrer
    if (this.props.dispatch) {
      event.preventDefault();
      const {
        altKey,
        button,
        ctrlKey,
        metaKey,
        shiftKey
      } = event;
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
        data: {
          event: {
            altKey,
            button,
            ctrlKey,
            metaKey,
            shiftKey
          },
          referrer: "https://getpocket.com/recommendations",
          // Use the anchor's url, which could have been cleaned up
          url: event.currentTarget.href
        }
      }));
    } // Propagate event if there's a handler


    if (this.props.onLinkClick) {
      this.props.onLinkClick(event);
    }
  }

  safeURI(url) {
    let protocol = null;

    try {
      protocol = new URL(url).protocol;
    } catch (e) {
      return "";
    }

    const isAllowed = ["http:", "https:"].includes(protocol);

    if (!isAllowed) {
      console.warn(`${url} is not allowed for anchor targets.`); // eslint-disable-line no-console

      return "";
    }

    return url;
  }

  render() {
    const {
      url,
      className
    } = this.props;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("a", {
      href: this.safeURI(url),
      className: className,
      onClick: this.onClick
    }, this.props.children);
  }

}

/***/ }),
/* 27 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "StatusMessage", function() { return StatusMessage; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SponsorLabel", function() { return SponsorLabel; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSContextFooter", function() { return DSContextFooter; });
/* harmony import */ var _Card_types_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(28);
/* harmony import */ var react_transition_group__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(29);
/* harmony import */ var react_transition_group__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_transition_group__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var _FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(30);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



 // Animation time is mirrored in DSContextFooter.scss

const ANIMATION_DURATION = 3000;
const StatusMessage = ({
  icon,
  fluentID
}) => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
  className: "status-message"
}, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
  "aria-haspopup": "true",
  className: `story-badge-icon icon icon-${icon}`
}), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
  className: "story-context-label",
  "data-l10n-id": fluentID
}));
const SponsorLabel = ({
  sponsored_by_override,
  sponsor,
  context
}) => {
  const classList = "story-sponsored-label clamp"; // If override is not false or an empty string.

  if (sponsored_by_override) {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: classList
    }, sponsored_by_override);
  } else if (sponsored_by_override === "") {
    // We specifically want to display nothing if the server returns an empty string.
    // So the server can turn off the label.
    // This is to support the use cases where the sponsored context is displayed elsewhere.
    return null;
  } else if (sponsor) {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: classList
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(_FluentOrText_FluentOrText_jsx__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
      message: {
        id: `newtab-label-sponsored-by`,
        values: {
          sponsor
        }
      }
    }));
  } else if (context) {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("p", {
      className: classList
    }, context);
  }

  return null;
};
class DSContextFooter extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  render() {
    // display_engagement_labels is based on pref `browser.newtabpage.activity-stream.discoverystream.engagementLabelEnabled`
    const {
      context,
      context_type,
      engagement,
      display_engagement_labels,
      sponsor,
      sponsored_by_override
    } = this.props;
    const {
      icon,
      fluentID
    } = _Card_types_js__WEBPACK_IMPORTED_MODULE_0__["cardContextTypes"][context_type] || {};
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "story-footer"
    }, SponsorLabel({
      sponsored_by_override,
      sponsor,
      context
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_transition_group__WEBPACK_IMPORTED_MODULE_1__["TransitionGroup"], {
      component: null
    }, !context && (context_type || display_engagement_labels && engagement) && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(react_transition_group__WEBPACK_IMPORTED_MODULE_1__["CSSTransition"], {
      key: fluentID,
      timeout: ANIMATION_DURATION,
      classNames: "story-animate"
    }, engagement && !context_type ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "story-view-count"
    }, engagement) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(StatusMessage, {
      icon: icon,
      fluentID: fluentID
    }))));
  }

}

/***/ }),
/* 28 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "cardContextTypes", function() { return cardContextTypes; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const cardContextTypes = {
  history: {
    fluentID: "newtab-label-visited",
    icon: "history-item"
  },
  removedBookmark: {
    fluentID: "newtab-label-removed-bookmark",
    icon: "bookmark-removed"
  },
  bookmark: {
    fluentID: "newtab-label-bookmarked",
    icon: "bookmark-added"
  },
  trending: {
    fluentID: "newtab-label-recommended",
    icon: "trending"
  },
  pocket: {
    fluentID: "newtab-label-saved",
    icon: "pocket"
  },
  download: {
    fluentID: "newtab-label-download",
    icon: "download"
  }
};

/***/ }),
/* 29 */
/***/ (function(module, exports) {

module.exports = ReactTransitionGroup;

/***/ }),
/* 30 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "FluentOrText", function() { return FluentOrText; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Set text on a child element/component depending on if the message is already
 * translated plain text or a fluent id with optional args.
 */

class FluentOrText extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    // Ensure we have a single child to attach attributes
    const {
      children,
      message
    } = this.props;
    const child = children ? react__WEBPACK_IMPORTED_MODULE_0___default.a.Children.only(children) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", null); // For a string message, just use it as the child's text

    let grandChildren = message;
    let extraProps; // Convert a message object to set desired fluent-dom attributes

    if (typeof message === "object") {
      const args = message.args || message.values;
      extraProps = {
        "data-l10n-args": args && JSON.stringify(args),
        "data-l10n-id": message.id || message.string_id
      }; // Use original children potentially with data-l10n-name attributes

      grandChildren = child.props.children;
    } // Add the message to the child via fluent attributes or text node


    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.cloneElement(child, extraProps, grandChildren);
  }

}

/***/ }),
/* 31 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSEmptyState", function() { return DSEmptyState; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class DSEmptyState extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onReset = this.onReset.bind(this);
    this.state = {};
  }

  componentWillUnmount() {
    if (this.timeout) {
      clearTimeout(this.timeout);
    }
  }

  onReset() {
    if (this.props.dispatch && this.props.feed) {
      const {
        feed
      } = this.props;
      const {
        url
      } = feed;
      this.props.dispatch({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_FEED_UPDATE,
        data: {
          feed: { ...feed,
            data: { ...feed.data,
              status: "waiting"
            }
          },
          url
        }
      });
      this.setState({
        waiting: true
      });
      this.timeout = setTimeout(() => {
        this.timeout = null;
        this.setState({
          waiting: false
        });
      }, 300);
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISCOVERY_STREAM_RETRY_FEED,
        data: {
          feed
        }
      }));
    }
  }

  renderButton() {
    if (this.props.status === "waiting" || this.state.waiting) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("button", {
        className: "try-again-button waiting",
        "data-l10n-id": "newtab-discovery-empty-section-topstories-loading"
      });
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("button", {
      className: "try-again-button",
      onClick: this.onReset,
      "data-l10n-id": "newtab-discovery-empty-section-topstories-try-again-button"
    });
  }

  renderState() {
    if (this.props.status === "waiting" || this.props.status === "failed") {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_1___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("h2", {
        "data-l10n-id": "newtab-discovery-empty-section-topstories-timed-out"
      }), this.renderButton());
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(react__WEBPACK_IMPORTED_MODULE_1___default.a.Fragment, null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("h2", {
      "data-l10n-id": "newtab-discovery-empty-section-topstories-header"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("p", {
      "data-l10n-id": "newtab-discovery-empty-section-topstories-content"
    }));
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "section-empty-state"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "empty-state-message"
    }, this.renderState()));
  }

}

/***/ }),
/* 32 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CollectionCardGrid", function() { return CollectionCardGrid; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_CardGrid_CardGrid__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(17);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSDismiss_DSDismiss__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(33);
/* harmony import */ var content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(23);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





class CollectionCardGrid extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onDismissClick = this.onDismissClick.bind(this);
    this.state = {
      dismissed: false
    };
  }

  onDismissClick() {
    const {
      data
    } = this.props;

    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      this.setState({
        dismissed: true
      });
      const pos = 0;
      const source = this.props.type.toUpperCase(); // Grab the available items in the array to dismiss.
      // This fires a ping for all items available, even if below the fold.

      const spocsData = data.spocs.map(item => ({
        url: item.url,
        guid: item.id,
        shim: item.shim,
        flight_id: item.flightId
      }));
      const blockUrlOption = content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_3__["LinkMenuOptions"].BlockUrls(spocsData, pos, source);
      const {
        action,
        impression,
        userEvent
      } = blockUrlOption;
      this.props.dispatch(action);
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: userEvent,
        source,
        action_position: pos
      }));

      if (impression) {
        this.props.dispatch(impression);
      }
    }
  }

  render() {
    const {
      data,
      dismissible
    } = this.props;

    if (this.state.dismissed || !data || !data.spocs || !data.spocs[0] || // We only display complete collections.
    data.spocs.length < 3) {
      return null;
    }

    const {
      spocs,
      placement,
      feed
    } = this.props; // spocs.data is spocs state data, and not an array of spocs.

    const {
      title,
      context,
      sponsored_by_override,
      sponsor
    } = spocs.data[placement.name] || {}; // Just in case of bad data, don't display a broken collection.

    if (!title) {
      return null;
    }

    let sponsoredByMessage = ""; // If override is not false or an empty string.

    if (sponsored_by_override || sponsored_by_override === "") {
      // We specifically want to display nothing if the server returns an empty string.
      // So the server can turn off the label.
      // This is to support the use cases where the sponsored context is displayed elsewhere.
      sponsoredByMessage = sponsored_by_override;
    } else if (sponsor) {
      sponsoredByMessage = {
        id: `newtab-label-sponsored-by`,
        values: {
          sponsor
        }
      };
    } else if (context) {
      sponsoredByMessage = context;
    } // Generally a card grid displays recs with spocs already injected.
    // Normally it doesn't care which rec is a spoc and which isn't,
    // it just displays content in a grid.
    // For collections, we're only displaying a list of spocs.
    // We don't need to tell the card grid that our list of cards are spocs,
    // it shouldn't need to care. So we just pass our spocs along as recs.
    // Think of it as injecting all rec positions with spocs.
    // Consider maybe making recommendations in CardGrid use a more generic name.


    const recsData = {
      recommendations: data.spocs
    }; // All cards inside of a collection card grid have a slightly different type.
    // For the case of interactions to the card grid, we use the type "COLLECTIONCARDGRID".
    // Example, you dismiss the whole collection, we use the type "COLLECTIONCARDGRID".
    // For interactions inside the card grid, example, you dismiss a single card in the collection,
    // we use the type "COLLECTIONCARDGRID_CARD".

    const type = `${this.props.type}_card`;
    const collectionGrid = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "ds-collection-card-grid"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_DiscoveryStreamComponents_CardGrid_CardGrid__WEBPACK_IMPORTED_MODULE_1__["CardGrid"], {
      title: title,
      context: sponsoredByMessage,
      data: recsData,
      feed: feed,
      border: this.props.border,
      type: type,
      is_collection: true,
      dispatch: this.props.dispatch,
      items: this.props.items
    }));

    if (dismissible) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSDismiss_DSDismiss__WEBPACK_IMPORTED_MODULE_2__["DSDismiss"], {
        onDismissClick: this.onDismissClick,
        extraClasses: `ds-dismiss-ds-collection`
      }, collectionGrid);
    }

    return collectionGrid;
  }

}

/***/ }),
/* 33 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSDismiss", function() { return DSDismiss; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class DSDismiss extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onDismissClick = this.onDismissClick.bind(this);
    this.onHover = this.onHover.bind(this);
    this.offHover = this.offHover.bind(this);
    this.state = {
      hovering: false
    };
  }

  onDismissClick() {
    if (this.props.onDismissClick) {
      this.props.onDismissClick();
    }
  }

  onHover() {
    this.setState({
      hovering: true
    });
  }

  offHover() {
    this.setState({
      hovering: false
    });
  }

  render() {
    let className = `ds-dismiss
      ${this.state.hovering ? ` hovering` : ``}
      ${this.props.extraClasses ? ` ${this.props.extraClasses}` : ``}`;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: className
    }, this.props.children, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", {
      className: "ds-dismiss-button",
      "data-l10n-id": "newtab-dismiss-button-tooltip",
      onHover: this.onHover,
      onClick: this.onDismissClick,
      onMouseEnter: this.onHover,
      onMouseLeave: this.offHover
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      className: "icon icon-dismiss"
    })));
  }

}

/***/ }),
/* 34 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_CollapsibleSection", function() { return _CollapsibleSection; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CollapsibleSection", function() { return CollapsibleSection; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(35);
/* harmony import */ var content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(30);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var content_src_components_SectionMenu_SectionMenu__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(37);
/* harmony import */ var content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(38);
/* harmony import */ var content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(24);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */








const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
class _CollapsibleSection extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.collapseOrExpandSection = this.collapseOrExpandSection.bind(this);
    this.onHeaderClick = this.onHeaderClick.bind(this);
    this.onKeyPress = this.onKeyPress.bind(this);
    this.onTransitionEnd = this.onTransitionEnd.bind(this);
    this.enableOrDisableAnimation = this.enableOrDisableAnimation.bind(this);
    this.onMenuButtonMouseEnter = this.onMenuButtonMouseEnter.bind(this);
    this.onMenuButtonMouseLeave = this.onMenuButtonMouseLeave.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.state = {
      enableAnimation: true,
      isAnimating: false,
      menuButtonHover: false,
      showContextMenu: false
    };
    this.setContextMenuButtonRef = this.setContextMenuButtonRef.bind(this);
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

  setContextMenuButtonRef(element) {
    this.contextMenuButtonRef = element;
  }

  componentDidMount() {
    if (!this.props.Prefs.values.featureConfig.newNewtabExperienceEnabled) {
      this.contextMenuButtonRef.addEventListener("mouseenter", this.onMenuButtonMouseEnter);
      this.contextMenuButtonRef.addEventListener("mouseleave", this.onMenuButtonMouseLeave);
    }
  }

  componentWillUnmount() {
    this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this.enableOrDisableAnimation);

    if (!this.props.Prefs.values.featureConfig.newNewtabExperienceEnabled) {
      this.contextMenuButtonRef.removeEventListener("mouseenter", this.onMenuButtonMouseEnter);
      this.contextMenuButtonRef.removeEventListener("mouseleave", this.onMenuButtonMouseLeave);
    }
  }

  enableOrDisableAnimation() {
    // Only animate the collapse/expand for visible tabs.
    const visible = this.props.document.visibilityState === VISIBLE;

    if (this.state.enableAnimation !== visible) {
      this.setState({
        enableAnimation: visible
      });
    }
  }

  onBodyMount(node) {
    this.sectionBody = node;
  }

  collapseOrExpandSection() {
    // If this.sectionBody is unset, it means that we're in some sort of error
    // state, probably displaying the error fallback, so we won't be able to
    // compute the height, and we don't want to persist the preference.
    if (!this.sectionBody) {
      return;
    } // Get the current height of the body so max-height transitions can work


    this.setState({
      isAnimating: true,
      maxHeight: `${this._getSectionBodyHeight()}px`
    });
    const {
      action
    } = content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_6__["SectionMenuOptions"].CheckCollapsed(this.props);
    this.props.dispatch(action);
  }

  onHeaderClick() {
    // If the new new tab experience pref is turned on,
    // sections should not be collapsible.
    // If this.sectionBody is unset, it means that we're in some sort of error
    // state, probably displaying the error fallback, so we won't be able to
    // compute the height, and we don't want to persist the preference.
    // If props.collapsed is undefined handler shouldn't do anything.
    if (this.props.Prefs.values.featureConfig.newNewtabExperienceEnabled || !this.sectionBody || this.props.collapsed === undefined) {
      return;
    }

    this.collapseOrExpandSection();
    const {
      userEvent
    } = content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_6__["SectionMenuOptions"].CheckCollapsed(this.props);
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event: userEvent,
      source: this.props.eventSource
    }));
  }

  onKeyPress(event) {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      this.onHeaderClick();
    }
  }

  _getSectionBodyHeight() {
    const div = this.sectionBody;

    if (div.style.display === "none") {
      // If the div isn't displayed, we can't get it's height. So we display it
      // to get the height (it doesn't show up because max-height is set to 0px
      // in CSS). We don't undo this because we are about to expand the section.
      div.style.display = "block";
    }

    return div.scrollHeight;
  }

  onTransitionEnd(event) {
    // Only update the animating state for our own transition (not a child's)
    if (event.target === event.currentTarget) {
      this.setState({
        isAnimating: false
      });
    }
  }

  renderIcon() {
    const {
      icon
    } = this.props;

    if (icon && icon.startsWith("moz-extension://")) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: "icon icon-small-spacer",
        style: {
          backgroundImage: `url('${icon}')`
        }
      });
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
      className: `icon icon-small-spacer icon-${icon || "webextension"}`
    });
  }

  onMenuButtonMouseEnter() {
    this.setState({
      menuButtonHover: true
    });
  }

  onMenuButtonMouseLeave() {
    this.setState({
      menuButtonHover: false
    });
  }

  onMenuUpdate(showContextMenu) {
    this.setState({
      showContextMenu
    });
  }

  render() {
    const isCollapsible = this.props.collapsed !== undefined;
    const isNewNewtabExperienceEnabled = this.props.Prefs.values.featureConfig.newNewtabExperienceEnabled; // If new new tab prefs are set to true, sections should not be
    // collapsible. Expand and make the section visible, if it has been
    // previously collapsed.

    if (isNewNewtabExperienceEnabled && this.props.collapsed) {
      this.collapseOrExpandSection();
    }

    const {
      enableAnimation,
      isAnimating,
      maxHeight,
      menuButtonHover,
      showContextMenu
    } = this.state;
    const {
      id,
      eventSource,
      collapsed,
      learnMore,
      title,
      extraMenuOptions,
      showPrefName,
      privacyNoticeURL,
      dispatch,
      isFixed,
      isFirst,
      isLast,
      isWebExtension
    } = this.props;
    const active = menuButtonHover || showContextMenu;
    let bodyStyle;

    if (isAnimating && !collapsed) {
      bodyStyle = {
        maxHeight
      };
    } else if (!isAnimating && collapsed) {
      bodyStyle = {
        display: "none"
      };
    }

    let titleStyle;

    if (this.props.hideTitle) {
      titleStyle = {
        visibility: "hidden"
      };
    }

    return (
      /*#__PURE__*/
      // TODO: Bug 1702140: re-enable this rule.
      // eslint-disable-next-line jsx-a11y/role-supports-aria-props
      react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("section", {
        className: `collapsible-section ${this.props.className}${enableAnimation ? " animation-enabled" : ""}${collapsed ? " collapsed" : ""}${active ? " active" : ""}`,
        "aria-expanded": !collapsed // Note: data-section-id is used for web extension api tests in mozilla central
        ,
        "data-section-id": id
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
        className: "section-top-bar"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("h3", {
        className: "section-title",
        style: titleStyle
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: "click-target-container"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: "click-target",
        role: "button",
        tabIndex: "0",
        onKeyPress: this.onKeyPress,
        onClick: this.onHeaderClick
      }, !isNewNewtabExperienceEnabled && this.renderIcon(), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
        message: title
      }), !isNewNewtabExperienceEnabled && isCollapsible && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        "data-l10n-id": collapsed ? "newtab-section-expand-section-label" : "newtab-section-collapse-section-label",
        className: `collapsible-arrow icon ${collapsed ? "icon-arrowhead-forward-small" : "icon-arrowhead-down-small"}`
      })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: "learn-more-link-wrapper"
      }, learnMore && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("span", {
        className: "learn-more-link"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
        message: learnMore.link.message
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("a", {
        href: learnMore.link.href
      })))))), !isNewNewtabExperienceEnabled && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_7__["ContextMenuButton"], {
        tooltip: "newtab-menu-section-tooltip",
        onUpdate: this.onMenuUpdate,
        refFunction: this.setContextMenuButtonRef
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_SectionMenu_SectionMenu__WEBPACK_IMPORTED_MODULE_5__["SectionMenu"], {
        id: id,
        extraOptions: extraMenuOptions,
        source: eventSource,
        showPrefName: showPrefName,
        privacyNoticeURL: privacyNoticeURL,
        collapsed: collapsed,
        isFixed: isFixed,
        isFirst: isFirst,
        isLast: isLast,
        dispatch: dispatch,
        isWebExtension: isWebExtension
      })))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_ErrorBoundary_ErrorBoundary__WEBPACK_IMPORTED_MODULE_1__["ErrorBoundary"], {
        className: "section-body-fallback"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
        className: `section-body${isAnimating ? " animating" : ""}`,
        onTransitionEnd: this.onTransitionEnd,
        ref: this.onBodyMount,
        style: bodyStyle
      }, this.props.children)))
    );
  }

}
_CollapsibleSection.defaultProps = {
  document: global.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden"
  }
};
const CollapsibleSection = Object(react_redux__WEBPACK_IMPORTED_MODULE_4__["connect"])(state => ({
  Prefs: state.Prefs
}))(_CollapsibleSection);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 35 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ErrorBoundaryFallback", function() { return ErrorBoundaryFallback; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ErrorBoundary", function() { return ErrorBoundary; });
/* harmony import */ var content_src_components_A11yLinkButton_A11yLinkButton__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(36);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


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
    } // "A11yLinkButton" to force normal link styling stuff (eg cursor on hover)


    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: className
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      "data-l10n-id": "newtab-error-fallback-info"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(content_src_components_A11yLinkButton_A11yLinkButton__WEBPACK_IMPORTED_MODULE_0__["A11yLinkButton"], {
      className: "reload-button",
      onClick: this.onClick,
      "data-l10n-id": "newtab-error-fallback-refresh-link"
    })));
  }

}
ErrorBoundaryFallback.defaultProps = {
  className: "as-error-fallback"
};
class ErrorBoundary extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      hasError: false
    };
  }

  componentDidCatch(error, info) {
    this.setState({
      hasError: true
    });
  }

  render() {
    if (!this.state.hasError) {
      return this.props.children;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(this.props.FallbackComponent, {
      className: this.props.className
    });
  }

}
ErrorBoundary.defaultProps = {
  FallbackComponent: ErrorBoundaryFallback
};

/***/ }),
/* 36 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "A11yLinkButton", function() { return A11yLinkButton; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function A11yLinkButton(props) {
  // function for merging classes, if necessary
  let className = "a11y-link-button";

  if (props.className) {
    className += ` ${props.className}`;
  }

  return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", _extends({
    type: "button"
  }, props, {
    className: className
  }), props.children);
}

/***/ }),
/* 37 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_SectionMenu", function() { return _SectionMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionMenu", function() { return SectionMenu; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(22);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(38);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





const DEFAULT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "RemoveSection", "CheckCollapsed", "Separator", "ManageSection"];
const WEBEXT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "CheckCollapsed", "Separator", "ManageWebExtension"];
class _SectionMenu extends react__WEBPACK_IMPORTED_MODULE_2___default.a.PureComponent {
  handleAddWhileCollapsed() {
    const {
      action,
      userEvent
    } = content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_4__["SectionMenuOptions"].ExpandSection(this.props);
    this.props.dispatch(action);

    if (userEvent) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: userEvent,
        source: this.props.source
      }));
    }
  }

  getOptions() {
    const {
      props
    } = this;
    const propOptions = props.isWebExtension ? [...WEBEXT_SECTION_MENU_OPTIONS] : [...DEFAULT_SECTION_MENU_OPTIONS]; // Remove Collapse/Expand related option if the `newNewtabExperience.enabled`
    // pref is set to true.

    if (props.Prefs.values.featureConfig.newNewtabExperienceEnabled) {
      if (props.isWebExtension) {
        propOptions.splice(2, 2);
      } else {
        propOptions.splice(4, 1);
      }
    } // Remove the move related options if the section is fixed


    if (props.isFixed) {
      propOptions.splice(propOptions.indexOf("MoveUp"), 3);
    } // Prepend custom options and a separator


    if (props.extraOptions) {
      propOptions.splice(0, 0, ...props.extraOptions, "Separator");
    } // Insert privacy notice before the last option ("ManageSection")


    if (props.privacyNoticeURL) {
      propOptions.splice(-1, 0, "PrivacyNotice");
    }

    const options = propOptions.map(o => content_src_lib_section_menu_options__WEBPACK_IMPORTED_MODULE_4__["SectionMenuOptions"][o](props)).map(option => {
      const {
        action,
        id,
        type,
        userEvent
      } = option;

      if (!type && id) {
        option.onClick = () => {
          const hasAddEvent = userEvent === "MENU_ADD_TOPSITE" || userEvent === "MENU_ADD_SEARCH";

          if (props.collapsed && hasAddEvent) {
            this.handleAddWhileCollapsed();
          }

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
    }); // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.

    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_2___default.a.createElement(content_src_components_ContextMenu_ContextMenu__WEBPACK_IMPORTED_MODULE_1__["ContextMenu"], {
      onUpdate: this.props.onUpdate,
      options: this.getOptions(),
      keyboardAccess: this.props.keyboardAccess
    });
  }

}
const SectionMenu = Object(react_redux__WEBPACK_IMPORTED_MODULE_3__["connect"])(state => ({
  Prefs: state.Prefs
}))(_SectionMenu);

/***/ }),
/* 38 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionMenuOptions", function() { return SectionMenuOptions; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * List of functions that return items that can be included as menu options in a
 * SectionMenu. All functions take the section as the only parameter.
 */

const SectionMenuOptions = {
  Separator: () => ({
    type: "separator"
  }),
  MoveUp: section => ({
    id: "newtab-section-menu-move-up",
    icon: "arrowhead-up",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SECTION_MOVE,
      data: {
        id: section.id,
        direction: -1
      }
    }),
    userEvent: "MENU_MOVE_UP",
    disabled: !!section.isFirst
  }),
  MoveDown: section => ({
    id: "newtab-section-menu-move-down",
    icon: "arrowhead-down",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SECTION_MOVE,
      data: {
        id: section.id,
        direction: +1
      }
    }),
    userEvent: "MENU_MOVE_DOWN",
    disabled: !!section.isLast
  }),
  RemoveSection: section => ({
    id: "newtab-section-menu-remove-section",
    icon: "dismiss",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].SetPref(section.showPrefName, false),
    userEvent: "MENU_REMOVE"
  }),
  CollapseSection: section => ({
    id: "newtab-section-menu-collapse-section",
    icon: "minimize",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].UPDATE_SECTION_PREFS,
      data: {
        id: section.id,
        value: {
          collapsed: true
        }
      }
    }),
    userEvent: "MENU_COLLAPSE"
  }),
  ExpandSection: section => ({
    id: "newtab-section-menu-expand-section",
    icon: "maximize",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].UPDATE_SECTION_PREFS,
      data: {
        id: section.id,
        value: {
          collapsed: false
        }
      }
    }),
    userEvent: "MENU_EXPAND"
  }),
  ManageSection: section => ({
    id: "newtab-section-menu-manage-section",
    icon: "settings",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SETTINGS_OPEN
    }),
    userEvent: "MENU_MANAGE"
  }),
  ManageWebExtension: section => ({
    id: "newtab-section-menu-manage-webext",
    icon: "settings",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_WEBEXT_SETTINGS,
      data: section.id
    })
  }),
  AddTopSite: section => ({
    id: "newtab-section-menu-add-topsite",
    icon: "add",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT,
      data: {
        index: -1
      }
    },
    userEvent: "MENU_ADD_TOPSITE"
  }),
  AddSearchShortcut: section => ({
    id: "newtab-section-menu-add-search-engine",
    icon: "search",
    action: {
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL
    },
    userEvent: "MENU_ADD_SEARCH"
  }),
  PrivacyNotice: section => ({
    id: "newtab-section-menu-privacy-notice",
    icon: "info",
    action: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
      data: {
        url: section.privacyNoticeURL
      }
    }),
    userEvent: "MENU_PRIVACY_NOTICE"
  }),
  CheckCollapsed: section => section.collapsed ? SectionMenuOptions.ExpandSection(section) : SectionMenuOptions.CollapseSection(section)
};

/***/ }),
/* 39 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSMessage", function() { return DSMessage; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(26);
/* harmony import */ var content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(30);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class DSMessage extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "ds-message"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("header", {
      className: "title"
    }, this.props.icon && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "glyph",
      style: {
        backgroundImage: `url(${this.props.icon})`
      }
    }), this.props.title && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      className: "title-text"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
      message: this.props.title
    })), this.props.link_text && this.props.link_url && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_1__["SafeAnchor"], {
      className: "link",
      url: this.props.link_url
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_2__["FluentOrText"], {
      message: this.props.link_text
    }))));
  }

}

/***/ }),
/* 40 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSPrivacyModal", function() { return DSPrivacyModal; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(1);
/* harmony import */ var content_src_asrouter_components_ModalOverlay_ModalOverlay__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(41);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class DSPrivacyModal extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.onLearnLinkClick = this.onLearnLinkClick.bind(this);
    this.onManageLinkClick = this.onManageLinkClick.bind(this);
  }

  onLearnLinkClick(event) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({
      event: "CLICK_PRIVACY_INFO",
      source: "DS_PRIVACY_MODAL"
    }));
  }

  onManageLinkClick(event) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionTypes"].SETTINGS_OPEN
    }));
  }

  closeModal() {
    this.props.dispatch({
      type: `HIDE_PRIVACY_INFO`,
      data: {}
    });
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(content_src_asrouter_components_ModalOverlay_ModalOverlay__WEBPACK_IMPORTED_MODULE_2__["ModalOverlayWrapper"], {
      onClose: this.closeModal,
      innerClassName: "ds-privacy-modal"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "privacy-notice"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("h3", {
      "data-l10n-id": "newtab-privacy-modal-header"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("p", {
      "data-l10n-id": "newtab-privacy-modal-paragraph-2"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("a", {
      className: "modal-link modal-link-privacy",
      "data-l10n-id": "newtab-privacy-modal-link",
      onClick: this.onLearnLinkClick,
      href: "https://help.getpocket.com/article/1142-firefox-new-tab-recommendations-faq"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", {
      className: "modal-link modal-link-manage",
      "data-l10n-id": "newtab-privacy-modal-button-manage",
      onClick: this.onManageLinkClick
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("section", {
      className: "actions"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("button", {
      className: "done",
      type: "submit",
      onClick: this.closeModal,
      "data-l10n-id": "newtab-privacy-modal-button-done"
    })));
  }

}

/***/ }),
/* 41 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ModalOverlayWrapper", function() { return ModalOverlayWrapper; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class ModalOverlayWrapper extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onKeyDown = this.onKeyDown.bind(this);
  } // The intended behaviour is to listen for an escape key
  // but not for a click; see Bug 1582242


  onKeyDown(event) {
    if (event.key === "Escape") {
      this.props.onClose(event);
    }
  }

  componentWillMount() {
    this.props.document.addEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.add("modal-open");
  }

  componentWillUnmount() {
    this.props.document.removeEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.remove("modal-open");
  }

  render() {
    const {
      props
    } = this;
    let className = props.unstyled ? "" : "modalOverlayInner active";

    if (props.innerClassName) {
      className += ` ${props.innerClassName}`;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "modalOverlayOuter active",
      onKeyDown: this.onKeyDown,
      role: "presentation"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: className,
      "aria-labelledby": props.headerId,
      id: props.id,
      role: "dialog"
    }, props.children));
  }

}
ModalOverlayWrapper.defaultProps = {
  document: global.document
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 42 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSSignup", function() { return DSSignup; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(21);
/* harmony import */ var content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(24);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(25);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(26);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






class DSSignup extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      active: false,
      lastItem: false
    };
    this.onMenuButtonUpdate = this.onMenuButtonUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
  }

  onMenuButtonUpdate(showContextMenu) {
    if (!showContextMenu) {
      this.setState({
        active: false,
        lastItem: false
      });
    }
  }

  nextAnimationFrame() {
    return new Promise(resolve => this.props.windowObj.requestAnimationFrame(resolve));
  }

  async onMenuShow() {
    let {
      lastItem
    } = this.state; // Wait for next frame before computing scrollMaxX to allow fluent menu strings to be visible

    await this.nextAnimationFrame();

    if (this.props.windowObj.scrollMaxX > 0) {
      lastItem = true;
    }

    this.setState({
      active: true,
      lastItem
    });
  }

  onLinkClick() {
    const {
      data
    } = this.props;

    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      const source = this.props.type.toUpperCase(); // Grab the first item in the array as we only have 1 spoc position.

      const [spoc] = data.spocs;
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "CLICK",
        source,
        action_position: 0
      }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
        source,
        click: 0,
        tiles: [{
          id: spoc.id,
          pos: 0,
          ...(spoc.shim && spoc.shim.click ? {
            shim: spoc.shim.click
          } : {})
        }]
      }));
    }
  }

  render() {
    const {
      data,
      dispatch,
      type
    } = this.props;

    if (!data || !data.spocs || !data.spocs[0]) {
      return null;
    } // Grab the first item in the array as we only have 1 spoc position.


    const [spoc] = data.spocs;
    const {
      title,
      url,
      excerpt,
      flight_id,
      id,
      shim
    } = spoc;
    const SIGNUP_CONTEXT_MENU_OPTIONS = ["OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", ...(flight_id ? ["ShowPrivacyInfo"] : [])];
    const outerClassName = ["ds-signup", this.state.active && "active", this.state.lastItem && "last-item"].filter(v => v).join(" ");
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: outerClassName
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "ds-signup-content"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", {
      className: "icon icon-small-spacer icon-mail"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", null, title, " ", /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_5__["SafeAnchor"], {
      className: "ds-chevron-link",
      dispatch: dispatch,
      onLinkClick: this.onLinkClick,
      url: url
    }, excerpt)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__["ImpressionStats"], {
      flightId: flight_id,
      rows: [{
        id,
        pos: 0,
        shim: shim && shim.impression
      }],
      dispatch: dispatch,
      source: type
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_2__["ContextMenuButton"], {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuButtonUpdate
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_1__["LinkMenu"], {
      dispatch: dispatch,
      index: 0,
      source: type.toUpperCase(),
      onShow: this.onMenuShow,
      options: SIGNUP_CONTEXT_MENU_OPTIONS,
      shouldSendImpressionStats: true,
      site: {
        referrer: "https://getpocket.com/recommendations",
        title,
        type,
        url,
        guid: id,
        shim,
        flight_id
      }
    })));
  }

}
DSSignup.defaultProps = {
  windowObj: window // Added to support unit tests

};

/***/ }),
/* 43 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DSTextPromo", function() { return DSTextPromo; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_DiscoveryStreamComponents_DSDismiss_DSDismiss__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(33);
/* harmony import */ var _DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(19);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(25);
/* harmony import */ var content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(23);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(26);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */







class DSTextPromo extends react__WEBPACK_IMPORTED_MODULE_5___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onDismissClick = this.onDismissClick.bind(this);
  }

  onLinkClick() {
    const {
      data
    } = this.props;

    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      const source = this.props.type.toUpperCase(); // Grab the first item in the array as we only have 1 spoc position.

      const [spoc] = data.spocs;
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "CLICK",
        source,
        action_position: 0
      }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
        source,
        click: 0,
        tiles: [{
          id: spoc.id,
          pos: 0,
          ...(spoc.shim && spoc.shim.click ? {
            shim: spoc.shim.click
          } : {})
        }]
      }));
    }
  }

  onDismissClick() {
    const {
      data
    } = this.props;

    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      const index = 0;
      const source = this.props.type.toUpperCase(); // Grab the first item in the array as we only have 1 spoc position.

      const [spoc] = data.spocs;
      const spocData = {
        url: spoc.url,
        guid: spoc.id,
        shim: spoc.shim
      };
      const blockUrlOption = content_src_lib_link_menu_options__WEBPACK_IMPORTED_MODULE_4__["LinkMenuOptions"].BlockUrl(spocData, index, source);
      const {
        action,
        impression,
        userEvent
      } = blockUrlOption;
      this.props.dispatch(action);
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: userEvent,
        source,
        action_position: index
      }));

      if (impression) {
        this.props.dispatch(impression);
      }
    }
  }

  render() {
    const {
      data
    } = this.props;

    if (!data || !data.spocs || !data.spocs[0]) {
      return null;
    } // Grab the first item in the array as we only have 1 spoc position.


    const [spoc] = data.spocs;
    const {
      image_src,
      raw_image_src,
      alt_text,
      title,
      url,
      context,
      cta,
      flight_id,
      id,
      shim
    } = spoc;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(content_src_components_DiscoveryStreamComponents_DSDismiss_DSDismiss__WEBPACK_IMPORTED_MODULE_1__["DSDismiss"], {
      onDismissClick: this.onDismissClick,
      extraClasses: `ds-dismiss-ds-text-promo`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "ds-text-promo"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(_DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_2__["DSImage"], {
      alt_text: alt_text,
      source: image_src,
      rawSource: raw_image_src
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "text"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("h3", null, `${title}\u2003`, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_6__["SafeAnchor"], {
      className: "ds-chevron-link",
      dispatch: this.props.dispatch,
      onLinkClick: this.onLinkClick,
      url: url
    }, cta)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("p", {
      className: "subtitle"
    }, context)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__["ImpressionStats"], {
      flightId: flight_id,
      rows: [{
        id,
        pos: 0,
        shim: shim && shim.impression
      }],
      dispatch: this.props.dispatch,
      source: this.props.type
    })));
  }

}

/***/ }),
/* 44 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Hero", function() { return Hero; });
/* harmony import */ var _DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(18);
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(1);
/* harmony import */ var _DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(31);
/* harmony import */ var _DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(19);
/* harmony import */ var _DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(20);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(25);
/* harmony import */ var _List_List_jsx__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(45);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_7___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_7__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(26);
/* harmony import */ var _DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(27);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */










class Hero extends react__WEBPACK_IMPORTED_MODULE_7___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].UserEvent({
        event: "CLICK",
        source: this.props.type.toUpperCase(),
        action_position: this.heroRec.pos
      }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_1__["actionCreators"].ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        tiles: [{
          id: this.heroRec.id,
          pos: this.heroRec.pos,
          ...(this.heroRec.shim && this.heroRec.shim.click ? {
            shim: this.heroRec.shim.click
          } : {})
        }]
      }));
    }
  }

  renderHero() {
    let [heroRec, ...otherRecs] = this.props.data.recommendations.slice(0, this.props.items);
    this.heroRec = heroRec;
    const cards = [];

    for (let index = 0; index < this.props.items - 1; index++) {
      const rec = otherRecs[index];
      cards.push(!rec || rec.placeholder ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__["PlaceholderDSCard"], {
        key: `dscard-${index}`
      }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__["DSCard"], {
        flightId: rec.flight_id,
        key: `dscard-${rec.id}`,
        image_src: rec.image_src,
        raw_image_src: rec.raw_image_src,
        title: rec.title,
        url: rec.url,
        id: rec.id,
        shim: rec.shim,
        pos: rec.pos,
        type: this.props.type,
        dispatch: this.props.dispatch,
        context: rec.context,
        context_type: rec.context_type,
        source: rec.domain,
        pocket_id: rec.pocket_id,
        bookmarkGuid: rec.bookmarkGuid,
        engagement: rec.engagement
      }));
    }

    let heroCard = null;

    if (!heroRec || heroRec.placeholder) {
      heroCard = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSCard_DSCard_jsx__WEBPACK_IMPORTED_MODULE_0__["PlaceholderDSCard"], null);
    } else {
      heroCard = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
        className: "ds-hero-item",
        key: `dscard-${heroRec.id}`
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_8__["SafeAnchor"], {
        className: "wrapper",
        dispatch: this.props.dispatch,
        onLinkClick: this.onLinkClick,
        url: heroRec.url
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
        className: "img-wrapper"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_3__["DSImage"], {
        extraClassNames: "img",
        source: heroRec.image_src,
        rawSource: heroRec.raw_image_src
      })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
        className: "meta"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
        className: "header-and-excerpt"
      }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("p", {
        className: "source clamp"
      }, heroRec.domain), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("header", {
        className: "clamp"
      }, heroRec.title), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("p", {
        className: "excerpt clamp"
      }, heroRec.excerpt)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_9__["DSContextFooter"], {
        context: heroRec.context,
        context_type: heroRec.context_type,
        engagement: heroRec.engagement
      })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_5__["ImpressionStats"], {
        flightId: heroRec.flight_id,
        rows: [{
          id: heroRec.id,
          pos: heroRec.pos,
          ...(heroRec.shim && heroRec.shim.impression ? {
            shim: heroRec.shim.impression
          } : {})
        }],
        dispatch: this.props.dispatch,
        source: this.props.type
      })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_4__["DSLinkMenu"], {
        id: heroRec.id,
        index: heroRec.pos,
        dispatch: this.props.dispatch,
        url: heroRec.url,
        title: heroRec.title,
        source: heroRec.domain,
        type: this.props.type,
        pocket_id: heroRec.pocket_id,
        shim: heroRec.shim,
        bookmarkGuid: heroRec.bookmarkGuid,
        flightId: heroRec.flight_id
      }));
    }

    let list = /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_List_List_jsx__WEBPACK_IMPORTED_MODULE_6__["List"], {
      recStartingPoint: 1,
      data: this.props.data,
      feed: this.props.feed,
      hasImages: true,
      hasBorders: this.props.border === `border`,
      items: this.props.items - 1,
      type: `Hero`
    });
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: `ds-hero ds-hero-${this.props.border}`
    }, heroCard, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: `${this.props.subComponentType}`
    }, this.props.subComponentType === `cards` ? cards : list));
  }

  render() {
    const {
      data
    } = this.props; // Handle a render before feed has been fetched by displaying nothing

    if (!data || !data.recommendations) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", null);
    } // Handle the case where a user has dismissed all recommendations


    const isEmpty = data.recommendations.length === 0;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: "ds-header"
    }, this.props.title), isEmpty ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement("div", {
      className: "ds-hero empty"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_7___default.a.createElement(_DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_2__["DSEmptyState"], {
      status: data.status,
      dispatch: this.props.dispatch,
      feed: this.props.feed
    })) : this.renderHero());
  }

}
Hero.defaultProps = {
  data: {},
  border: `border`,
  items: 1 // Number of stories to display

};

/***/ }),
/* 45 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ListItem", function() { return ListItem; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PlaceholderListItem", function() { return PlaceholderListItem; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_List", function() { return _List; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "List", function() { return List; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var _DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(31);
/* harmony import */ var _DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(19);
/* harmony import */ var _DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(20);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(25);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(26);
/* harmony import */ var _DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(27);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */









/**
 * @note exported for testing only
 */

class ListItem extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
  // TODO performance: get feeds to send appropriately sized images rather
  // than waiting longer and scaling down on client?
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "CLICK",
        source: this.props.type.toUpperCase(),
        action_position: this.props.pos,
        value: {
          card_type: this.props.flightId ? "spoc" : "organic"
        }
      }));
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        tiles: [{
          id: this.props.id,
          pos: this.props.pos,
          ...(this.props.shim && this.props.shim.click ? {
            shim: this.props.shim.click
          } : {})
        }]
      }));
    }
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("li", {
      className: `ds-list-item${this.props.placeholder ? " placeholder" : ""}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_7__["SafeAnchor"], {
      className: "ds-list-item-link",
      dispatch: this.props.dispatch,
      onLinkClick: !this.props.placeholder ? this.onLinkClick : undefined,
      url: this.props.url
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "ds-list-item-text"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("p", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("span", {
      className: "ds-list-item-info clamp"
    }, this.props.domain)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "ds-list-item-body"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "ds-list-item-title clamp"
    }, this.props.title), this.props.excerpt && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "ds-list-item-excerpt clamp"
    }, this.props.excerpt)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_DSContextFooter_DSContextFooter_jsx__WEBPACK_IMPORTED_MODULE_8__["DSContextFooter"], {
      context: this.props.context,
      context_type: this.props.context_type,
      engagement: this.props.engagement
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_DSImage_DSImage_jsx__WEBPACK_IMPORTED_MODULE_3__["DSImage"], {
      extraClassNames: "ds-list-image",
      source: this.props.image_src,
      rawSource: this.props.raw_image_src
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_5__["ImpressionStats"], {
      flightId: this.props.flightId,
      rows: [{
        id: this.props.id,
        pos: this.props.pos,
        ...(this.props.shim && this.props.shim.impression ? {
          shim: this.props.shim.impression
        } : {})
      }],
      dispatch: this.props.dispatch,
      source: this.props.type
    })), !this.props.placeholder && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_DSLinkMenu_DSLinkMenu__WEBPACK_IMPORTED_MODULE_4__["DSLinkMenu"], {
      id: this.props.id,
      index: this.props.pos,
      dispatch: this.props.dispatch,
      url: this.props.url,
      title: this.props.title,
      source: this.props.source,
      type: this.props.type,
      pocket_id: this.props.pocket_id,
      shim: this.props.shim,
      bookmarkGuid: this.props.bookmarkGuid,
      flightId: this.props.flightId
    }));
  }

}
const PlaceholderListItem = props => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(ListItem, {
  placeholder: true
});
/**
 * @note exported for testing only
 */

function _List(props) {
  const renderList = () => {
    const recs = props.data.recommendations.slice(props.recStartingPoint, props.recStartingPoint + props.items);
    const recMarkup = [];

    for (let index = 0; index < props.items; index++) {
      const rec = recs[index];
      recMarkup.push(!rec || rec.placeholder ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(PlaceholderListItem, {
        key: `ds-list-item-${index}`
      }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(ListItem, {
        key: `ds-list-item-${rec.id}`,
        dispatch: props.dispatch,
        flightId: rec.flight_id,
        domain: rec.domain,
        excerpt: rec.excerpt,
        id: rec.id,
        shim: rec.shim,
        image_src: rec.image_src,
        raw_image_src: rec.raw_image_src,
        pos: rec.pos,
        title: rec.title,
        context: rec.context,
        context_type: rec.context_type,
        type: props.type,
        url: rec.url,
        pocket_id: rec.pocket_id,
        bookmarkGuid: rec.bookmarkGuid,
        engagement: rec.engagement
      }));
    }

    const listStyles = ["ds-list", props.fullWidth ? "ds-list-full-width" : "", props.hasBorders ? "ds-list-borders" : "", props.hasImages ? "ds-list-images" : "", props.hasNumbers ? "ds-list-numbers" : ""];
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("ul", {
      className: listStyles.join(" ")
    }, recMarkup);
  };

  const {
    data
  } = props;

  if (!data || !data.recommendations) {
    return null;
  } // Handle the case where a user has dismissed all recommendations


  const isEmpty = data.recommendations.length === 0;
  return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", null, props.header && props.header.title ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
    className: "ds-header"
  }, props.header.title) : null, isEmpty ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
    className: "ds-list empty"
  }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_DSEmptyState_DSEmptyState_jsx__WEBPACK_IMPORTED_MODULE_2__["DSEmptyState"], {
    status: data.status,
    dispatch: props.dispatch,
    feed: props.feed
  })) : renderList());
}
_List.defaultProps = {
  recStartingPoint: 0,
  // Index of recommendations to start displaying from
  fullWidth: false,
  // Display items taking up the whole column
  hasBorders: false,
  // Display lines separating each item
  hasImages: false,
  // Display images for each item
  hasNumbers: false,
  // Display numbers for each item
  items: 6 // Number of stories to display.  TODO: get from endpoint

};
const List = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(state => ({
  DiscoveryStream: state.DiscoveryStream
}))(_List);

/***/ }),
/* 46 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Highlights", function() { return _Highlights; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Highlights", function() { return Highlights; });
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(47);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class _Highlights extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const section = this.props.Sections.find(s => s.id === "highlights");

    if (!section || !section.enabled) {
      return null;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "ds-highlights sections-list"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(content_src_components_Sections_Sections__WEBPACK_IMPORTED_MODULE_2__["SectionIntl"], _extends({}, section, {
      isFixed: true
    })));
  }

}
const Highlights = Object(react_redux__WEBPACK_IMPORTED_MODULE_0__["connect"])(state => ({
  Sections: state.Sections
}))(_Highlights);

/***/ }),
/* 47 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Section", function() { return Section; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionIntl", function() { return SectionIntl; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Sections", function() { return _Sections; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Sections", function() { return Sections; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(48);
/* harmony import */ var content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(34);
/* harmony import */ var content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(50);
/* harmony import */ var content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(30);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var content_src_components_MoreRecommendations_MoreRecommendations__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(52);
/* harmony import */ var content_src_components_PocketLoggedInCta_PocketLoggedInCta__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(53);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_8___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_8__);
/* harmony import */ var content_src_components_Topics_Topics__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(54);
/* harmony import */ var content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(55);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */











const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const CARDS_PER_ROW_DEFAULT = 3;
const CARDS_PER_ROW_COMPACT_WIDE = 4;
class Section extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  get numRows() {
    const {
      rowsPref,
      maxRows,
      Prefs
    } = this.props;
    return rowsPref ? Prefs.values[rowsPref] : maxRows;
  }

  _dispatchImpressionStats() {
    const {
      props
    } = this;
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
      props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
        source: props.eventSource,
        tiles: cards.map(link => ({
          id: link.guid
        }))
      }));
      this.impressionCardGuids = cards.map(link => link.guid);
    }
  } // This sends an event when a user sees a set of new content. If content
  // changes while the page is hidden (i.e. preloaded or on a hidden tab),
  // only send the event if the page becomes visible again.


  sendImpressionStatsOrAddListener() {
    const {
      props
    } = this;

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
      } // When the page becomes visible, send the impression stats ping if the section isn't collapsed.


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

  componentWillMount() {
    this.sendNewTabRehydrated(this.props.initialized);
  }

  componentDidMount() {
    if (this.props.rows.length && !this.props.pref.collapsed) {
      this.sendImpressionStatsOrAddListener();
    }
  }

  componentDidUpdate(prevProps) {
    const {
      props
    } = this;
    const isCollapsed = props.pref.collapsed;
    const wasCollapsed = prevProps.pref.collapsed;

    if ( // Don't send impression stats for the empty state
    props.rows.length && ( // We only want to send impression stats if the content of the cards has changed
    // and the section is not collapsed...
    props.rows !== prevProps.rows && !isCollapsed || // or if we are expanding a section that was collapsed.
    wasCollapsed && !isCollapsed)) {
      this.sendImpressionStatsOrAddListener();
    }
  }

  componentWillUpdate(nextProps) {
    this.sendNewTabRehydrated(nextProps.initialized);
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
  } // The NEW_TAB_REHYDRATED event is used to inform feeds that their
  // data has been consumed e.g. for counting the number of tabs that
  // have rendered that data.


  sendNewTabRehydrated(initialized) {
    if (initialized && !this.renderNotified) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_REHYDRATED,
        data: {}
      }));
      this.renderNotified = true;
    }
  }

  render() {
    const {
      id,
      eventSource,
      title,
      icon,
      rows,
      Pocket,
      topics,
      emptyState,
      dispatch,
      compactCards,
      read_more_endpoint,
      contextMenuOptions,
      initialized,
      learnMore,
      pref,
      privacyNoticeURL,
      isFirst,
      isLast
    } = this.props;
    const waitingForSpoc = id === "topstories" && this.props.Pocket.waitingForSpoc;
    const maxCardsPerRow = compactCards ? CARDS_PER_ROW_COMPACT_WIDE : CARDS_PER_ROW_DEFAULT;
    const {
      numRows
    } = this;
    const maxCards = maxCardsPerRow * numRows;
    const maxCardsOnNarrow = CARDS_PER_ROW_DEFAULT * numRows;
    const {
      pocketCta,
      isUserLoggedIn
    } = Pocket || {};
    const {
      useCta
    } = pocketCta || {}; // Don't display anything until we have a definitve result from Pocket,
    // to avoid a flash of logged out state while we render.

    const isPocketLoggedInDefined = isUserLoggedIn === true || isUserLoggedIn === false;
    const hasTopics = topics && !!topics.length;
    const shouldShowPocketCta = id === "topstories" && useCta && isUserLoggedIn === false; // Show topics only for top stories and if it has loaded with topics.
    // The classs .top-stories-bottom-container ensures content doesn't shift as things load.

    const shouldShowTopics = id === "topstories" && hasTopics && (useCta && isUserLoggedIn === true || !useCta && isPocketLoggedInDefined); // We use topics to determine language support for read more.

    const shouldShowReadMore = read_more_endpoint && hasTopics;
    const realRows = rows.slice(0, maxCards); // The empty state should only be shown after we have initialized and there is no content.
    // Otherwise, we should show placeholders.

    const shouldShowEmptyState = initialized && !rows.length;
    const cards = [];

    if (!shouldShowEmptyState) {
      for (let i = 0; i < maxCards; i++) {
        const link = realRows[i]; // On narrow viewports, we only show 3 cards per row. We'll mark the rest as
        // .hide-for-narrow to hide in CSS via @media query.

        const className = i >= maxCardsOnNarrow ? "hide-for-narrow" : "";
        let usePlaceholder = !link; // If we are in the third card and waiting for spoc,
        // use the placeholder.

        if (!usePlaceholder && i === 2 && waitingForSpoc) {
          usePlaceholder = true;
        }

        cards.push(!usePlaceholder ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_1__["Card"], {
          key: i,
          index: i,
          className: className,
          dispatch: dispatch,
          link: link,
          contextMenuOptions: contextMenuOptions,
          eventSource: eventSource,
          shouldSendImpressionStats: this.props.shouldSendImpressionStats,
          isWebExtension: this.props.isWebExtension
        }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Card_Card__WEBPACK_IMPORTED_MODULE_1__["PlaceholderCard"], {
          key: i,
          className: className
        }));
      }
    }

    const sectionClassName = ["section", compactCards ? "compact-cards" : "normal-cards"].join(" "); // <Section> <-- React component
    // <section> <-- HTML5 element

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__["ComponentPerfTimer"], this.props, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__["CollapsibleSection"], {
      className: sectionClassName,
      icon: icon,
      title: title,
      id: id,
      eventSource: eventSource,
      collapsed: this.props.pref.collapsed,
      showPrefName: pref && pref.feed || id,
      privacyNoticeURL: privacyNoticeURL,
      Prefs: this.props.Prefs,
      isFixed: this.props.isFixed,
      isFirst: isFirst,
      isLast: isLast,
      learnMore: learnMore,
      dispatch: this.props.dispatch,
      isWebExtension: this.props.isWebExtension
    }, !shouldShowEmptyState && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("ul", {
      className: "section-list",
      style: {
        padding: 0
      }
    }, cards), shouldShowEmptyState && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "section-empty-state"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "empty-state"
    }, emptyState.icon && emptyState.icon.startsWith("moz-extension://") ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("span", {
      className: "empty-state-icon icon",
      style: {
        "background-image": `url('${emptyState.icon}')`
      }
    }) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("span", {
      className: `empty-state-icon icon icon-${emptyState.icon}`
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_4__["FluentOrText"], {
      message: emptyState.message
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("p", {
      className: "empty-state-message"
    })))), id === "topstories" && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "top-stories-bottom-container"
    }, shouldShowTopics && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "wrapper-topics"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_Topics_Topics__WEBPACK_IMPORTED_MODULE_9__["Topics"], {
      topics: this.props.topics
    })), shouldShowPocketCta && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "wrapper-cta"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_PocketLoggedInCta_PocketLoggedInCta__WEBPACK_IMPORTED_MODULE_7__["PocketLoggedInCta"], null)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "wrapper-more-recommendations"
    }, shouldShowReadMore && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_MoreRecommendations_MoreRecommendations__WEBPACK_IMPORTED_MODULE_6__["MoreRecommendations"], {
      read_more_endpoint: read_more_endpoint
    })))));
  }

}
Section.defaultProps = {
  document: global.document,
  rows: [],
  emptyState: {},
  pref: {},
  title: ""
};
const SectionIntl = Object(react_redux__WEBPACK_IMPORTED_MODULE_5__["connect"])(state => ({
  Prefs: state.Prefs,
  Pocket: state.Pocket
}))(Section);
class _Sections extends react__WEBPACK_IMPORTED_MODULE_8___default.a.PureComponent {
  renderSections() {
    const sections = [];
    const enabledSections = this.props.Sections.filter(section => section.enabled);
    const {
      sectionOrder,
      "feeds.topsites": showTopSites
    } = this.props.Prefs.values; // Enabled sections doesn't include Top Sites, so we add it if enabled.

    const expectedCount = enabledSections.length + ~~showTopSites;

    for (const sectionId of sectionOrder.split(",")) {
      const commonProps = {
        key: sectionId,
        isFirst: sections.length === 0,
        isLast: sections.length === expectedCount - 1
      };

      if (sectionId === "topsites" && showTopSites) {
        sections.push( /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_10__["TopSites"], commonProps));
      } else {
        const section = enabledSections.find(s => s.id === sectionId);

        if (section) {
          sections.push( /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement(SectionIntl, _extends({}, section, commonProps)));
        }
      }
    }

    return sections;
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_8___default.a.createElement("div", {
      className: "sections-list"
    }, this.renderSections());
  }

}
const Sections = Object(react_redux__WEBPACK_IMPORTED_MODULE_5__["connect"])(state => ({
  Sections: state.Sections,
  Prefs: state.Prefs
}))(_Sections);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 48 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Card", function() { return _Card; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Card", function() { return Card; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PlaceholderCard", function() { return PlaceholderCard; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(28);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(24);
/* harmony import */ var content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(21);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_5___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_5__);
/* harmony import */ var content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(49);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






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

class _Card extends react__WEBPACK_IMPORTED_MODULE_5___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      activeCard: null,
      imageLoaded: false,
      cardImage: null
    };
    this.onMenuButtonUpdate = this.onMenuButtonUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }
  /**
   * Helper to conditionally load an image and update state when it loads.
   */


  async maybeLoadImage() {
    // No need to load if it's already loaded or no image
    const {
      cardImage
    } = this.state;

    if (!cardImage) {
      return;
    }

    const imageUrl = cardImage.url;

    if (!this.state.imageLoaded) {
      // Initialize a promise to share a load across multiple card updates
      if (!gImageLoading.has(imageUrl)) {
        const loaderPromise = new Promise((resolve, reject) => {
          const loader = new Image();
          loader.addEventListener("load", resolve);
          loader.addEventListener("error", reject);
          loader.src = imageUrl;
        }); // Save and remove the promise only while it's pending

        gImageLoading.set(imageUrl, loaderPromise);
        loaderPromise.catch(ex => ex).then(() => gImageLoading.delete(imageUrl)).catch();
      } // Wait for the image whether just started loading or reused promise


      try {
        await gImageLoading.get(imageUrl);
      } catch (ex) {
        // Ignore the failed image without changing state
        return;
      } // Only update state if we're still waiting to load the original image


      if (content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__["ScreenshotUtils"].isRemoteImageLocal(this.state.cardImage, this.props.link.image) && !this.state.imageLoaded) {
        this.setState({
          imageLoaded: true
        });
      }
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
    const {
      image
    } = nextProps.link;
    const imageInState = content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__["ScreenshotUtils"].isRemoteImageLocal(prevState.cardImage, image);
    let nextState = null; // Image is updating.

    if (!imageInState && nextProps.link) {
      nextState = {
        imageLoaded: false
      };
    }

    if (imageInState) {
      return nextState;
    } // Since image was updated, attempt to revoke old image blob URL, if it exists.


    content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__["ScreenshotUtils"].maybeRevokeBlobObjectURL(prevState.cardImage);
    nextState = nextState || {};
    nextState.cardImage = content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__["ScreenshotUtils"].createLocalImageObject(image);
    return nextState;
  }

  onMenuButtonUpdate(isOpen) {
    if (isOpen) {
      this.setState({
        activeCard: this.props.index
      });
    } else {
      this.setState({
        activeCard: null
      });
    }
  }
  /**
   * Report to telemetry additional information about the item.
   */


  _getTelemetryInfo() {
    // Filter out "history" type for being the default
    if (this.props.link.type !== "history") {
      return {
        value: {
          card_type: this.props.link.type
        }
      };
    }

    return null;
  }

  onLinkClick(event) {
    event.preventDefault();
    const {
      altKey,
      button,
      ctrlKey,
      metaKey,
      shiftKey
    } = event;

    if (this.props.link.type === "download") {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_DOWNLOAD_FILE,
        data: Object.assign(this.props.link, {
          event: {
            button,
            ctrlKey,
            metaKey,
            shiftKey
          }
        })
      }));
    } else {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
        data: Object.assign(this.props.link, {
          event: {
            altKey,
            button,
            ctrlKey,
            metaKey,
            shiftKey
          }
        })
      }));
    }

    if (this.props.isWebExtension) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].WebExtEvent(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].WEBEXT_CLICK, {
        source: this.props.eventSource,
        url: this.props.link.url,
        action_position: this.props.index
      }));
    } else {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(Object.assign({
        event: "CLICK",
        source: this.props.eventSource,
        action_position: this.props.index
      }, this._getTelemetryInfo())));

      if (this.props.shouldSendImpressionStats) {
        this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
          source: this.props.eventSource,
          click: 0,
          tiles: [{
            id: this.props.link.guid,
            pos: this.props.index
          }]
        }));
      }
    }
  }

  componentDidMount() {
    this.maybeLoadImage();
  }

  componentDidUpdate() {
    this.maybeLoadImage();
  } // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.


  componentWillMount() {
    const nextState = _Card.getNextStateFromProps(this.props, this.state);

    if (nextState) {
      this.setState(nextState);
    }
  } // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.


  componentWillReceiveProps(nextProps) {
    const nextState = _Card.getNextStateFromProps(nextProps, this.state);

    if (nextState) {
      this.setState(nextState);
    }
  }

  componentWillUnmount() {
    content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_6__["ScreenshotUtils"].maybeRevokeBlobObjectURL(this.state.cardImage);
  }

  render() {
    const {
      index,
      className,
      link,
      dispatch,
      contextMenuOptions,
      eventSource,
      shouldSendImpressionStats
    } = this.props;
    const {
      props
    } = this;
    const title = link.title || link.hostname;
    const isContextMenuOpen = this.state.activeCard === index; // Display "now" as "trending" until we have new strings #3402

    const {
      icon,
      fluentID
    } = _types__WEBPACK_IMPORTED_MODULE_1__["cardContextTypes"][link.type === "now" ? "trending" : link.type] || {};
    const hasImage = this.state.cardImage || link.hasImage;
    const imageStyle = {
      backgroundImage: this.state.cardImage ? `url(${this.state.cardImage.url})` : "none"
    };
    const outerClassName = ["card-outer", className, isContextMenuOpen && "active", props.placeholder && "placeholder"].filter(v => v).join(" ");
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("li", {
      className: outerClassName
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("a", {
      href: link.type === "pocket" ? link.open_url : link.url,
      onClick: !props.placeholder ? this.onLinkClick : undefined
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-preview-image-outer"
    }, hasImage && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: `card-preview-image${this.state.imageLoaded ? " loaded" : ""}`,
      style: imageStyle
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-details"
    }, link.type === "download" && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-host-name alternate",
      "data-l10n-id": "newtab-menu-open-file"
    }), link.hostname && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-host-name"
    }, link.hostname.slice(0, 100), link.type === "download" && `  \u2014 ${link.description}`), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: ["card-text", icon ? "" : "no-context", link.description ? "" : "no-description", link.hostname ? "" : "no-host-name"].join(" ")
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("h4", {
      className: "card-title",
      dir: "auto"
    }, link.title), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("p", {
      className: "card-description",
      dir: "auto"
    }, link.description)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-context"
    }, icon && !link.context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("span", {
      "aria-haspopup": "true",
      className: `card-context-icon icon icon-${icon}`
    }), link.icon && link.context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("span", {
      "aria-haspopup": "true",
      className: "card-context-icon icon",
      style: {
        backgroundImage: `url('${link.icon}')`
      }
    }), fluentID && !link.context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-context-label",
      "data-l10n-id": fluentID
    }), link.context && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement("div", {
      className: "card-context-label"
    }, link.context))))), !props.placeholder && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_3__["ContextMenuButton"], {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuButtonUpdate
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_4__["LinkMenu"], {
      dispatch: dispatch,
      index: index,
      source: eventSource,
      options: link.contextMenuOptions || contextMenuOptions,
      site: link,
      siteInfo: this._getTelemetryInfo(),
      shouldSendImpressionStats: shouldSendImpressionStats
    })));
  }

}
_Card.defaultProps = {
  link: {}
};
const Card = Object(react_redux__WEBPACK_IMPORTED_MODULE_2__["connect"])(state => ({
  platform: state.Prefs.values.platform
}))(_Card);
const PlaceholderCard = props => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_5___default.a.createElement(Card, {
  placeholder: true,
  className: props.className
});

/***/ }),
/* 49 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ScreenshotUtils", function() { return ScreenshotUtils; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      return {
        url: global.URL.createObjectURL(remoteImage.data),
        path: remoteImage.path
      };
    }

    return {
      url: remoteImage
    };
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
    } // This will only handle the remaining three possible outcomes.
    // (i.e. everything except when both image and localImage are present)


    return !remoteImage && !localImage;
  }

};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 50 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ComponentPerfTimer", function() { return ComponentPerfTimer; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_lib_perf_service__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(51);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


 // Currently record only a fixed set of sections. This will prevent data
// from custom sections from showing up or from topstories.

const RECORDED_SECTIONS = ["highlights", "topsites"];
class ComponentPerfTimer extends react__WEBPACK_IMPORTED_MODULE_2___default.a.Component {
  constructor(props) {
    super(props); // Just for test dependency injection:

    this.perfSvc = this.props.perfSvc || content_src_lib_perf_service__WEBPACK_IMPORTED_MODULE_1__["perfService"];
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
      this._reportMissingData = false; // Report how long it took for component to become initialized.

      this._sendBadStateEvent();
    }
  }

  _maybeSendPaintedEvent() {
    // If we've already handled a timestamp, don't do it again.
    if (this._timestampHandled || !this.props.initialized) {
      return;
    } // And if we haven't, we're doing so now, so remember that. Even if
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
      this._recordedFirstRender = true; // topsites_first_render_ts, highlights_first_render_ts.

      const key = `${this.props.id}_first_render_ts`;
      this.perfSvc.mark(key);
    }
  }
  /**
   * Creates `SAVE_SESSION_PERF_DATA` with timestamp in ms
   * of how much longer the data took to be ready for display than it would
   * have been the ideal case.
   * https://github.com/mozilla/ping-centre/issues/98
   */


  _sendBadStateEvent() {
    // highlights_data_ready_ts, topsites_data_ready_ts.
    const dataReadyKey = `${this.props.id}_data_ready_ts`;
    this.perfSvc.mark(dataReadyKey);

    try {
      const firstRenderKey = `${this.props.id}_first_render_ts`; // value has to be Int32.

      const value = parseInt(this.perfSvc.getMostRecentAbsMarkStartByName(dataReadyKey) - this.perfSvc.getMostRecentAbsMarkStartByName(firstRenderKey), 10);
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
        // highlights_data_late_by_ms, topsites_data_late_by_ms.
        data: {
          [`${this.props.id}_data_late_by_ms`]: value
        }
      }));
    } catch (ex) {// If this failed, it's likely because the `privacy.resistFingerprinting`
      // pref is true.
    }
  }

  _sendPaintedEvent() {
    // Record first_painted event but only send if topsites.
    if (this.props.id !== "topsites") {
      return;
    } // topsites_first_painted_ts.


    const key = `${this.props.id}_first_painted_ts`;
    this.perfSvc.mark(key);

    try {
      const data = {};
      data[key] = this.perfSvc.getMostRecentAbsMarkStartByName(key);
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
        data
      }));
    } catch (ex) {// If this failed, it's likely because the `privacy.resistFingerprinting`
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
/* 51 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_PerfService", function() { return _PerfService; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "perfService", function() { return perfService; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


let usablePerfObj = window.performance;
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
const perfService = new _PerfService();

/***/ }),
/* 52 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MoreRecommendations", function() { return MoreRecommendations; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class MoreRecommendations extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    const {
      read_more_endpoint
    } = this.props;

    if (read_more_endpoint) {
      return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("a", {
        className: "more-recommendations",
        href: read_more_endpoint,
        "data-l10n-id": "newtab-pocket-more-recommendations"
      });
    }

    return null;
  }

}

/***/ }),
/* 53 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_PocketLoggedInCta", function() { return _PocketLoggedInCta; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "PocketLoggedInCta", function() { return PocketLoggedInCta; });
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class _PocketLoggedInCta extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const {
      pocketCta
    } = this.props.Pocket;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      className: "pocket-logged-in-cta"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("a", {
      className: "pocket-cta-button",
      href: pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"
    }, pocketCta.ctaButton ? pocketCta.ctaButton : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      "data-l10n-id": "newtab-pocket-cta-button"
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("a", {
      href: pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      className: "cta-text"
    }, pocketCta.ctaText ? pocketCta.ctaText : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      "data-l10n-id": "newtab-pocket-cta-text"
    }))));
  }

}
const PocketLoggedInCta = Object(react_redux__WEBPACK_IMPORTED_MODULE_0__["connect"])(state => ({
  Pocket: state.Pocket
}))(_PocketLoggedInCta);

/***/ }),
/* 54 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Topic", function() { return Topic; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Topics", function() { return Topics; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class Topic extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    const {
      url,
      name
    } = this.props;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("li", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("a", {
      key: name,
      href: url
    }, name));
  }

}
class Topics extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    const {
      topics
    } = this.props;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      className: "topics"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("span", {
      "data-l10n-id": "newtab-pocket-read-more"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("ul", null, topics && topics.map(t => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement(Topic, {
      key: t.name,
      url: t.url,
      name: t.name
    }))));
  }

}

/***/ }),
/* 55 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_TopSites", function() { return _TopSites; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSites", function() { return TopSites; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(56);
/* harmony import */ var content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(34);
/* harmony import */ var content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(50);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var _asrouter_components_ModalOverlay_ModalOverlay__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(41);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_6___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_6__);
/* harmony import */ var _SearchShortcutsForm__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(57);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(73);
/* harmony import */ var _TopSiteForm__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(74);
/* harmony import */ var _TopSite__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(58);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */












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
    custom_screenshot: 0,
    screenshot_with_icon: 0,
    screenshot: 0,
    tippytop: 0,
    rich_icon: 0,
    no_image: 0
  });
}

class _TopSites extends react__WEBPACK_IMPORTED_MODULE_6___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onEditFormClose = this.onEditFormClose.bind(this);
    this.onSearchShortcutsFormClose = this.onSearchShortcutsFormClose.bind(this);
  }
  /**
   * Dispatch session statistics about the quality of TopSites icons and pinned count.
   */


  _dispatchTopSitesStats() {
    const topSites = this._getVisibleTopSites().filter(topSite => topSite !== null && topSite !== undefined);

    const topSitesIconsStats = countTopSitesIconsTypes(topSites);
    const topSitesPinned = topSites.filter(site => !!site.isPinned).length;
    const searchShortcuts = topSites.filter(site => !!site.searchTopSite).length; // Dispatch telemetry event with the count of TopSites images types.

    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA,
      data: {
        topsites_icon_stats: topSitesIconsStats,
        topsites_pinned: topSitesPinned,
        topsites_search_shortcuts: searchShortcuts
      }
    }));
  }
  /**
   * Return the TopSites that are visible based on prefs and window width.
   */


  _getVisibleTopSites() {
    // We hide 2 sites per row when not in the wide layout.
    let sitesPerRow = common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_8__["TOP_SITES_MAX_SITES_PER_ROW"]; // $break-point-widest = 1072px (from _variables.scss)

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

  onEditFormClose() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
      event: "TOP_SITES_EDIT_CLOSE"
    }));
    this.props.dispatch({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_CANCEL_EDIT
    });
  }

  onSearchShortcutsFormClose() {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
      event: "SEARCH_EDIT_CLOSE"
    }));
    this.props.dispatch({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL
    });
  }

  render() {
    const {
      props
    } = this;
    const {
      editForm,
      showSearchShortcutsForm
    } = props.TopSites;
    const extraMenuOptions = ["AddTopSite"];
    const {
      customizationMenuEnabled,
      newNewtabExperienceEnabled
    } = props.Prefs.values.featureConfig;
    const colors = props.Prefs.values["newNewtabExperience.colors"];

    if (props.Prefs.values["improvesearch.topSiteSearchShortcuts"]) {
      extraMenuOptions.push("AddSearchShortcut");
    }

    const canShowCustomizationMenu = newNewtabExperienceEnabled || customizationMenuEnabled;
    const hideTitle = props.Prefs.values.hideTopSitesTitle || canShowCustomizationMenu; // `collapsed` should be sent to CollapsibleSection as undefined if
    // `props.TopSites.pref` is not set to true.

    let collapsed;

    if (props.TopSites.pref) {
      collapsed = canShowCustomizationMenu ? false : props.TopSites.pref.collapsed;
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_ComponentPerfTimer_ComponentPerfTimer__WEBPACK_IMPORTED_MODULE_3__["ComponentPerfTimer"], {
      id: "topsites",
      initialized: props.TopSites.initialized,
      dispatch: props.dispatch
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(content_src_components_CollapsibleSection_CollapsibleSection__WEBPACK_IMPORTED_MODULE_2__["CollapsibleSection"], {
      className: "top-sites",
      icon: "topsites",
      id: "topsites",
      title: props.title || {
        id: "newtab-section-header-topsites"
      },
      hideTitle: hideTitle,
      extraMenuOptions: extraMenuOptions,
      showPrefName: "feeds.topsites",
      eventSource: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
      collapsed: collapsed,
      isFixed: props.isFixed,
      isFirst: props.isFirst,
      isLast: props.isLast,
      dispatch: props.dispatch
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_TopSite__WEBPACK_IMPORTED_MODULE_10__["TopSiteList"], {
      TopSites: props.TopSites,
      TopSitesRows: props.TopSitesRows,
      dispatch: props.dispatch,
      topSiteIconType: topSiteIconType,
      newNewtabExperienceEnabled: newNewtabExperienceEnabled,
      colors: colors
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "edit-topsites-wrapper"
    }, editForm && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "edit-topsites"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_asrouter_components_ModalOverlay_ModalOverlay__WEBPACK_IMPORTED_MODULE_5__["ModalOverlayWrapper"], {
      unstyled: true,
      onClose: this.onEditFormClose,
      innerClassName: "modal"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_TopSiteForm__WEBPACK_IMPORTED_MODULE_9__["TopSiteForm"], _extends({
      site: props.TopSites.rows[editForm.index],
      onClose: this.onEditFormClose,
      dispatch: this.props.dispatch
    }, editForm, {
      newNewtabExperienceEnabled: newNewtabExperienceEnabled,
      customizationMenuEnabled: customizationMenuEnabled
    })))), showSearchShortcutsForm && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement("div", {
      className: "edit-search-shortcuts"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_asrouter_components_ModalOverlay_ModalOverlay__WEBPACK_IMPORTED_MODULE_5__["ModalOverlayWrapper"], {
      unstyled: true,
      onClose: this.onSearchShortcutsFormClose,
      innerClassName: "modal"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_6___default.a.createElement(_SearchShortcutsForm__WEBPACK_IMPORTED_MODULE_7__["SearchShortcutsForm"], {
      TopSites: props.TopSites,
      onClose: this.onSearchShortcutsFormClose,
      dispatch: this.props.dispatch
    }))))));
  }

}
const TopSites = Object(react_redux__WEBPACK_IMPORTED_MODULE_4__["connect"])((state, props) => ({
  // For SPOC Experiment only, take TopSites from DiscoveryStream TopSites that takes in SPOC Data
  TopSites: props.TopSitesWithSpoc || state.TopSites,
  Prefs: state.Prefs,
  TopSitesRows: state.Prefs.values.topSitesRows
}))(_TopSites);
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 56 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_SOURCE", function() { return TOP_SITES_SOURCE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_CONTEXT_MENU_OPTIONS", function() { return TOP_SITES_CONTEXT_MENU_OPTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS", function() { return TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS", function() { return TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS", function() { return TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MIN_RICH_FAVICON_SIZE", function() { return MIN_RICH_FAVICON_SIZE; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "MIN_CORNER_FAVICON_SIZE", function() { return MIN_CORNER_FAVICON_SIZE; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const TOP_SITES_SOURCE = "TOP_SITES";
const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];
const TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS = ["PinTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "ShowPrivacyInfo"];
const TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS = ["OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "AboutSponsored"]; // the special top site for search shortcut experiment can only have the option to unpin (which removes) the topsite

const TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "BlockUrl"]; // minimum size necessary to show a rich icon instead of a screenshot

const MIN_RICH_FAVICON_SIZE = 96; // minimum size necessary to show any icon in the top left corner with a screenshot

const MIN_CORNER_FAVICON_SIZE = 16;

/***/ }),
/* 57 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SelectableSearchShortcut", function() { return SelectableSearchShortcut; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SearchShortcutsForm", function() { return SearchShortcutsForm; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(56);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class SelectableSearchShortcut extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const {
      shortcut,
      selected
    } = this.props;
    const imageStyle = {
      backgroundImage: `url("${shortcut.tippyTopIcon}")`
    };
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "top-site-outer search-shortcut"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("input", {
      type: "checkbox",
      id: shortcut.keyword,
      name: shortcut.keyword,
      checked: selected,
      onChange: this.props.onChange
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("label", {
      htmlFor: shortcut.keyword
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "top-site-inner"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "tile"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "top-site-icon rich-icon",
      style: imageStyle,
      "data-fallback": "@"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "top-site-icon search-topsite"
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "title"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      dir: "auto"
    }, shortcut.keyword))))));
  }

}
class SearchShortcutsForm extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onSaveButtonClick = this.onSaveButtonClick.bind(this); // clone the shortcuts and add them to the state so we can add isSelected property

    const shortcuts = [];
    const {
      rows,
      searchShortcuts
    } = props.TopSites;
    searchShortcuts.forEach(shortcut => {
      shortcuts.push({ ...shortcut,
        isSelected: !!rows.find(row => row && row.isPinned && row.searchTopSite && row.label === shortcut.keyword)
      });
    });
    this.state = {
      shortcuts
    };
  }

  handleChange(event) {
    const {
      target
    } = event;
    const {
      name,
      checked
    } = target;
    this.setState(prevState => {
      const shortcuts = prevState.shortcuts.slice();
      let shortcut = shortcuts.find(({
        keyword
      }) => keyword === name);
      shortcut.isSelected = checked;
      return {
        shortcuts
      };
    });
  }

  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }

  onSaveButtonClick(ev) {
    ev.preventDefault(); // Check if there were any changes and act accordingly

    const {
      rows
    } = this.props.TopSites;
    const pinQueue = [];
    const unpinQueue = [];
    this.state.shortcuts.forEach(shortcut => {
      const alreadyPinned = rows.find(row => row && row.isPinned && row.searchTopSite && row.label === shortcut.keyword);

      if (shortcut.isSelected && !alreadyPinned) {
        pinQueue.push(this._searchTopSite(shortcut));
      } else if (!shortcut.isSelected && alreadyPinned) {
        unpinQueue.push({
          url: alreadyPinned.url,
          searchVendor: shortcut.shortURL
        });
      }
    }); // Tell the feed to do the work.

    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].UPDATE_PINNED_SEARCH_SHORTCUTS,
      data: {
        addedShortcuts: pinQueue,
        deletedShortcuts: unpinQueue
      }
    })); // Send the Telemetry pings.

    pinQueue.forEach(shortcut => {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_SOURCE"],
        event: "SEARCH_EDIT_ADD",
        value: {
          search_vendor: shortcut.searchVendor
        }
      }));
    });
    unpinQueue.forEach(shortcut => {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_SOURCE"],
        event: "SEARCH_EDIT_DELETE",
        value: {
          search_vendor: shortcut.searchVendor
        }
      }));
    });
    this.props.onClose();
  }

  _searchTopSite(shortcut) {
    return {
      url: shortcut.url,
      searchTopSite: true,
      label: shortcut.keyword,
      searchVendor: shortcut.shortURL
    };
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("form", {
      className: "topsite-form"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: "search-shortcuts-container"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("h3", {
      className: "section-title grey-title",
      "data-l10n-id": "newtab-topsites-add-search-engine-header"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", null, this.state.shortcuts.map(shortcut => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(SelectableSearchShortcut, {
      key: shortcut.keyword,
      shortcut: shortcut,
      selected: shortcut.isSelected,
      onChange: this.handleChange
    })))), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("section", {
      className: "actions"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("button", {
      className: "cancel",
      type: "button",
      onClick: this.onCancelButtonClick,
      "data-l10n-id": "newtab-topsites-cancel-button"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("button", {
      className: "done",
      type: "submit",
      onClick: this.onSaveButtonClick,
      "data-l10n-id": "newtab-topsites-save-button"
    })));
  }

}

/***/ }),
/* 58 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteLink", function() { return TopSiteLink; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSite", function() { return TopSite; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSitePlaceholder", function() { return TopSitePlaceholder; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteList", function() { return TopSiteList; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(56);
/* harmony import */ var content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(21);
/* harmony import */ var _DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(25);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_4___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_4__);
/* harmony import */ var content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(49);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(73);
/* harmony import */ var content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(24);
/* harmony import */ var _TopSiteImpressionWrapper__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(59);
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */









const SPOC_TYPE = "SPOC";
const NEWTAB_SOURCE = "newtab";
class TopSiteLink extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      screenshotImage: null
    };
    this.onDragEvent = this.onDragEvent.bind(this);
    this.onKeyPress = this.onKeyPress.bind(this);
  }
  /*
   * Helper to determine whether the drop zone should allow a drop. We only allow
   * dropping top sites for now. We don't allow dropping on sponsored top sites
   * as their position is fixed.
   */


  _allowDrop(e) {
    return (this.dragged || !this.props.link.sponsored_position) && e.dataTransfer.types.includes("text/topsite-index");
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
        event.target.blur();

        if (this.props.link.sponsored_position) {
          event.preventDefault();
          break;
        }

        this.dragged = true;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/topsite-index", this.props.index);
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
        // Block the scroll wheel from appearing for middle clicks on search top sites
        if (event.button === 1 && this.props.link.searchTopSite) {
          event.preventDefault();
        } // Reset at the first mouse event of a potential drag


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
    const {
      screenshot
    } = nextProps.link;
    const imageInState = content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].isRemoteImageLocal(prevState.screenshotImage, screenshot);

    if (imageInState) {
      return null;
    } // Since image was updated, attempt to revoke old image blob URL, if it exists.


    content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].maybeRevokeBlobObjectURL(prevState.screenshotImage);
    return {
      screenshotImage: content_src_lib_screenshot_utils__WEBPACK_IMPORTED_MODULE_5__["ScreenshotUtils"].createLocalImageObject(screenshot)
    };
  } // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.


  componentWillMount() {
    const nextState = TopSiteLink.getNextStateFromProps(this.props, this.state);

    if (nextState) {
      this.setState(nextState);
    }
  } // NOTE: Remove this function when we update React to >= 16.3 since React will
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

  onKeyPress(event) {
    // If we have tabbed to a search shortcut top site, and we click 'enter',
    // we should execute the onClick function. This needs to be added because
    // search top sites are anchor tags without an href. See bug 1483135
    if (this.props.link.searchTopSite && event.key === "Enter") {
      this.props.onClick(event);
    }
  }
  /*
   * Takes the url as a string, runs it through a simple (non-secure) hash turning it into a random number
   * Apply that random number to the color array. The same url will always generate the same color.
   */


  generateColor() {
    let {
      title,
      colors
    } = this.props;

    if (!colors) {
      return "";
    }

    let colorArray = colors.split(",");

    const hashStr = str => {
      let hash = 0;

      for (let i = 0; i < str.length; i++) {
        let charCode = str.charCodeAt(i);
        hash += charCode;
      }

      return hash;
    };

    let hash = hashStr(title);
    let index = hash % colorArray.length;
    return colorArray[index];
  }

  calculateStyle() {
    const {
      defaultStyle,
      link,
      newNewtabExperienceEnabled
    } = this.props;
    const {
      tippyTopIcon,
      faviconSize
    } = link;
    let imageClassName;
    let imageStyle;
    let showSmallFavicon = false;
    let smallFaviconStyle;
    let smallFaviconFallback;
    let hasScreenshotImage = this.state.screenshotImage && this.state.screenshotImage.url;
    let selectedColor;

    if (defaultStyle) {
      // force no styles (letter fallback) even if the link has imagery
      smallFaviconFallback = false;

      if (newNewtabExperienceEnabled) {
        selectedColor = this.generateColor();
      }
    } else if (link.searchTopSite) {
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon})`
      };
      smallFaviconStyle = {
        backgroundImage: `url(${tippyTopIcon})`
      };
    } else if (link.customScreenshotURL) {
      // assume high quality custom screenshot and use rich icon styles and class names
      // TopSite spoc experiment only
      const spocImgURL = link.type === SPOC_TYPE ? link.customScreenshotURL : "";
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: hasScreenshotImage ? `url(${this.state.screenshotImage.url})` : `url(${spocImgURL})`
      };
    } else if (tippyTopIcon || faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["MIN_RICH_FAVICON_SIZE"]) {
      // styles and class names for top sites with rich icons
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon || link.favicon})`
      };
    } else {
      // styles and class names for top sites with screenshot + small icon in top left corner
      imageClassName = `screenshot${hasScreenshotImage ? " active" : ""}`;
      imageStyle = {
        backgroundImage: hasScreenshotImage ? `url(${this.state.screenshotImage.url})` : "none"
      }; // only show a favicon in top left if it's greater than 16x16

      if (faviconSize >= _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["MIN_CORNER_FAVICON_SIZE"]) {
        showSmallFavicon = true;
        smallFaviconStyle = {
          backgroundImage: `url(${link.favicon})`
        };
      } else if (newNewtabExperienceEnabled) {
        selectedColor = this.generateColor();
        imageClassName = "";
      } else if (hasScreenshotImage) {
        // Don't show a small favicon if there is no screenshot, because that
        // would result in two fallback icons
        showSmallFavicon = true;
        smallFaviconFallback = true;
      }
    }

    return {
      showSmallFavicon,
      smallFaviconFallback,
      smallFaviconStyle,
      imageStyle,
      imageClassName,
      selectedColor
    };
  }

  render() {
    const {
      children,
      className,
      isDraggable,
      link,
      onClick,
      title,
      newNewtabExperienceEnabled
    } = this.props;
    const topSiteOuterClassName = `top-site-outer${className ? ` ${className}` : ""}${link.isDragged ? " dragged" : ""}${link.searchTopSite ? " search-shortcut" : ""}`;
    const [letterFallback] = title;
    const {
      showSmallFavicon,
      smallFaviconFallback,
      smallFaviconStyle,
      imageStyle,
      imageClassName,
      selectedColor
    } = this.calculateStyle();
    let draggableProps = {};

    if (isDraggable) {
      draggableProps = {
        onClick: this.onDragEvent,
        onDragEnd: this.onDragEvent,
        onDragStart: this.onDragEvent,
        onMouseDown: this.onDragEvent
      };
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("li", _extends({
      className: topSiteOuterClassName,
      onDrop: this.onDragEvent,
      onDragOver: this.onDragEvent,
      onDragEnter: this.onDragEvent,
      onDragLeave: this.onDragEvent
    }, draggableProps), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "top-site-inner"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("a", {
      className: "top-site-button",
      href: link.searchTopSite ? undefined : link.url,
      tabIndex: "0",
      onKeyPress: this.onKeyPress,
      onClick: onClick,
      draggable: true
    }, newNewtabExperienceEnabled && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "tile",
      "aria-hidden": true
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: selectedColor ? "icon-wrapper letter-fallback" : "icon-wrapper",
      "data-fallback": letterFallback,
      style: selectedColor ? {
        backgroundColor: selectedColor
      } : {}
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: imageClassName,
      style: imageStyle
    }), showSmallFavicon && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "top-site-icon default-icon",
      "data-fallback": smallFaviconFallback && letterFallback,
      style: smallFaviconStyle
    })), link.searchTopSite && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "top-site-icon search-topsite"
    })) || /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "tile",
      "aria-hidden": true,
      "data-fallback": letterFallback
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: imageClassName,
      style: imageStyle
    }), link.searchTopSite && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "top-site-icon search-topsite"
    }), showSmallFavicon && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "top-site-icon default-icon",
      "data-fallback": smallFaviconFallback && letterFallback,
      style: smallFaviconStyle
    })), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: `title${link.isPinned ? " has-icon pinned" : ""}${link.type === SPOC_TYPE || link.show_sponsored_label ? " sponsored" : ""}`
    }, newNewtabExperienceEnabled && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", {
      dir: "auto"
    }, link.isPinned && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "icon icon-pin-small"
    }), title || /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("br", null), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", {
      className: "sponsored-label",
      "data-l10n-id": "newtab-topsite-sponsored"
    })) || /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", null, link.isPinned && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", {
      className: "icon icon-pin-small"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", {
      dir: "auto"
    }, title || /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("br", null)), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("span", {
      className: "sponsored-label",
      "data-l10n-id": "newtab-topsite-sponsored"
    })))), children, link.type === SPOC_TYPE ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_DiscoveryStreamImpressionStats_ImpressionStats__WEBPACK_IMPORTED_MODULE_3__["ImpressionStats"], {
      flightId: link.flightId,
      rows: [{
        id: link.id,
        pos: link.pos,
        shim: link.shim && link.shim.impression
      }],
      dispatch: this.props.dispatch,
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"]
    }) : null, link.sponsored_position ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(_TopSiteImpressionWrapper__WEBPACK_IMPORTED_MODULE_8__["TopSiteImpressionWrapper"], {
      tile: {
        position: this.props.index + 1,
        tile_id: link.sponsored_tile_id || -1,
        reporting_url: link.sponsored_impression_url,
        advertiser: title.toLocaleLowerCase(),
        source: NEWTAB_SOURCE
      },
      dispatch: this.props.dispatch
    }) : null));
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
    this.state = {
      showContextMenu: false
    };
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
  }
  /**
   * Report to telemetry additional information about the item.
   */


  _getTelemetryInfo() {
    const value = {
      icon_type: this.props.link.iconType
    }; // Filter out "not_pinned" type for being the default

    if (this.props.link.isPinned) {
      value.card_type = "pinned";
    }

    if (this.props.link.searchTopSite) {
      // Set the card_type as "search" regardless of its pinning status
      value.card_type = "search";
      value.search_vendor = this.props.link.hostname;
    }

    if (this.props.link.type === SPOC_TYPE || this.props.link.sponsored_position) {
      value.card_type = "spoc";
    }

    return {
      value
    };
  }

  userEvent(event) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent(Object.assign({
      event,
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
      action_position: this.props.index
    }, this._getTelemetryInfo())));
  }

  onLinkClick(event) {
    this.userEvent("CLICK"); // Specially handle a top site link click for "typed" frecency bonus as
    // specified as a property on the link.

    event.preventDefault();
    const {
      altKey,
      button,
      ctrlKey,
      metaKey,
      shiftKey
    } = event;

    if (!this.props.link.searchTopSite) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].OPEN_LINK,
        data: Object.assign(this.props.link, {
          event: {
            altKey,
            button,
            ctrlKey,
            metaKey,
            shiftKey
          }
        })
      })); // Fire off a spoc specific impression.

      if (this.props.link.type === SPOC_TYPE) {
        this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].ImpressionStats({
          source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
          click: 0,
          tiles: [{
            id: this.props.link.id,
            pos: this.props.link.pos,
            shim: this.props.link.shim && this.props.link.shim.click
          }]
        }));
      }

      if (this.props.link.sendAttributionRequest) {
        this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
          type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].PARTNER_LINK_ATTRIBUTION,
          data: {
            targetURL: this.props.link.url,
            source: "newtab"
          }
        }));
      }

      if (this.props.link.sponsored_position) {
        const title = this.props.link.label || this.props.link.hostname;
        this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
          type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_IMPRESSION_STATS,
          data: {
            type: "click",
            position: this.props.index + 1,
            tile_id: this.props.link.sponsored_tile_id || -1,
            reporting_url: this.props.link.sponsored_click_url,
            advertiser: title.toLocaleLowerCase(),
            source: NEWTAB_SOURCE
          }
        }));
      }
    } else {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].FILL_SEARCH_TERM,
        data: {
          label: this.props.link.label
        }
      }));
    }
  }

  onMenuUpdate(isOpen) {
    if (isOpen) {
      this.props.onActivate(this.props.index);
    } else {
      this.props.onActivate();
    }
  }

  render() {
    const {
      props
    } = this;
    const {
      link
    } = props;
    const isContextMenuOpen = props.activeIndex === props.index;
    const title = link.label || link.hostname;
    let menuOptions;

    if (link.sponsored_position) {
      menuOptions = _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS"];
    } else if (link.searchTopSite) {
      menuOptions = _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS"];
    } else if (link.type === SPOC_TYPE) {
      menuOptions = _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS"];
    } else {
      menuOptions = _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_CONTEXT_MENU_OPTIONS"];
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSiteLink, _extends({}, props, {
      onClick: this.onLinkClick,
      onDragEvent: this.props.onDragEvent,
      className: `${props.className || ""}${isContextMenuOpen ? " active" : ""}`,
      title: title
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("div", null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_ContextMenu_ContextMenuButton__WEBPACK_IMPORTED_MODULE_7__["ContextMenuButton"], {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuUpdate
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(content_src_components_LinkMenu_LinkMenu__WEBPACK_IMPORTED_MODULE_2__["LinkMenu"], {
      dispatch: props.dispatch,
      index: props.index,
      onUpdate: this.onMenuUpdate,
      options: menuOptions,
      site: link,
      shouldSendImpressionStats: link.type === SPOC_TYPE,
      siteInfo: this._getTelemetryInfo(),
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"]
    }))));
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
    this.props.dispatch({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_EDIT,
      data: {
        index: this.props.index
      }
    });
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSiteLink, _extends({}, this.props, {
      className: `placeholder ${this.props.className || ""}`,
      isDraggable: false
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("button", {
      "aria-haspopup": "true",
      className: "context-menu-button edit-button icon",
      "data-l10n-id": "newtab-menu-topsites-placeholder-tooltip",
      onClick: this.onEditButtonClick
    }));
  }

}
class TopSiteList extends react__WEBPACK_IMPORTED_MODULE_4___default.a.PureComponent {
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
    this.state = TopSiteList.DEFAULT_STATE;
    this.onDragEvent = this.onDragEvent.bind(this);
    this.onActivate = this.onActivate.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (this.state.draggedSite) {
      const prevTopSites = this.props.TopSites && this.props.TopSites.rows;
      const newTopSites = nextProps.TopSites && nextProps.TopSites.rows;

      if (prevTopSites && prevTopSites[this.state.draggedIndex] && prevTopSites[this.state.draggedIndex].url === this.state.draggedSite.url && (!newTopSites[this.state.draggedIndex] || newTopSites[this.state.draggedIndex].url !== this.state.draggedSite.url)) {
        // We got the new order from the redux store via props. We can clear state now.
        this.setState(TopSiteList.DEFAULT_STATE);
      }
    }
  }

  userEvent(event, index) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event,
      source: _TopSitesConstants__WEBPACK_IMPORTED_MODULE_1__["TOP_SITES_SOURCE"],
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
          this.setState(TopSiteList.DEFAULT_STATE);
        }

        break;

      case "dragenter":
        if (index === this.state.draggedIndex) {
          this.setState({
            topSitesPreview: null
          });
        } else {
          this.setState({
            topSitesPreview: this._makeTopSitesPreview(index)
          });
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
                customScreenshotURL: this.state.draggedSite.customScreenshotURL,
                // Only if the search topsites experiment is enabled
                ...(this.state.draggedSite.searchTopSite && {
                  searchTopSite: true
                })
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
    const preview = topSites.map(site => site && (site.isPinned || site.sponsored_position) ? site : null);
    const unpinned = topSites.filter(site => site && !site.isPinned && !site.sponsored_position);
    const siteToInsert = Object.assign({}, this.state.draggedSite, {
      isPinned: true,
      isDragged: true
    });

    if (!preview[index]) {
      preview[index] = siteToInsert;
    } else {
      // Find the hole to shift the pinned site(s) towards. We shift towards the
      // hole left by the site being dragged.
      let holeIndex = index;
      const indexStep = index > this.state.draggedIndex ? -1 : 1;

      while (preview[holeIndex]) {
        holeIndex += indexStep;
      } // Shift towards the hole.


      const shiftingStep = index > this.state.draggedIndex ? 1 : -1;

      while (index > this.state.draggedIndex ? holeIndex < index : holeIndex > index) {
        let nextIndex = holeIndex + shiftingStep;

        while (preview[nextIndex] && preview[nextIndex].sponsored_position) {
          nextIndex += shiftingStep;
        }

        preview[holeIndex] = preview[nextIndex];
        holeIndex = nextIndex;
      }

      preview[index] = siteToInsert;
    } // Fill in the remaining holes with unpinned sites.


    for (let i = 0; i < preview.length; i++) {
      if (!preview[i]) {
        preview[i] = unpinned.shift() || null;
      }
    }

    return preview;
  }

  onActivate(index) {
    this.setState({
      activeIndex: index
    });
  }

  render() {
    const {
      props
    } = this;

    const topSites = this.state.topSitesPreview || this._getTopSites();

    const topSitesUI = [];
    const commonProps = {
      onDragEvent: this.onDragEvent,
      dispatch: props.dispatch,
      newNewtabExperienceEnabled: props.newNewtabExperienceEnabled
    }; // We assign a key to each placeholder slot. We need it to be independent
    // of the slot index (i below) so that the keys used stay the same during
    // drag and drop reordering and the underlying DOM nodes are reused.
    // This mostly (only?) affects linux so be sure to test on linux before changing.

    let holeIndex = 0; // On narrow viewports, we only show 6 sites per row. We'll mark the rest as
    // .hide-for-narrow to hide in CSS via @media query.

    const maxNarrowVisibleIndex = props.TopSitesRows * 6;

    for (let i = 0, l = topSites.length; i < l; i++) {
      const link = topSites[i] && Object.assign({}, topSites[i], {
        iconType: this.props.topSiteIconType(topSites[i])
      });
      const slotProps = {
        key: link ? link.url : holeIndex++,
        index: i
      };

      if (i >= maxNarrowVisibleIndex) {
        slotProps.className = "hide-for-narrow";
      }

      topSitesUI.push(!link ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSitePlaceholder, _extends({}, slotProps, commonProps)) : /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement(TopSite, _extends({
        link: link,
        activeIndex: this.state.activeIndex,
        onActivate: this.onActivate
      }, slotProps, commonProps, {
        colors: props.colors
      })));
    }

    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_4___default.a.createElement("ul", {
      className: `top-sites-list${this.state.draggedSite ? " dnd-active" : ""}`
    }, topSitesUI);
  }

}

/***/ }),
/* 59 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "INTERSECTION_RATIO", function() { return INTERSECTION_RATIO; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteImpressionWrapper", function() { return TopSiteImpressionWrapper; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange"; // Per analytical requirement, we set the minimal intersection ratio to
// 0.5, and an impression is identified when the wrapped item has at least
// 50% visibility.
//
// This constant is exported for unit test

const INTERSECTION_RATIO = 0.5;
/**
 * Impression wrapper for a TopSite tile.
 *
 * It makses use of the Intersection Observer API to detect the visibility,
 * and relies on page visibility to ensure the impression is reported
 * only when the component is visible on the page.
 */

class TopSiteImpressionWrapper extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  _dispatchImpressionStats() {
    const {
      tile
    } = this.props;
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].TOP_SITES_IMPRESSION_STATS,
      data: {
        type: "impression",
        ...tile
      }
    }));
  }

  setImpressionObserverOrAddListener() {
    const {
      props
    } = this;

    if (!props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
      this.setImpressionObserver();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      this._onVisibilityChange = () => {
        if (props.document.visibilityState === VISIBLE) {
          this.setImpressionObserver();
          props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };

      props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }
  /**
   * Set an impression observer for the wrapped component. It makes use of
   * the Intersection Observer API to detect if the wrapped component is
   * visible with a desired ratio, and only sends impression if that's the case.
   *
   * See more details about Intersection Observer API at:
   * https://developer.mozilla.org/en-US/docs/Web/API/Intersection_Observer_API
   */


  setImpressionObserver() {
    const {
      props
    } = this;

    if (!props.tile) {
      return;
    }

    this._handleIntersect = entries => {
      if (entries.some(entry => entry.isIntersecting && entry.intersectionRatio >= INTERSECTION_RATIO)) {
        this._dispatchImpressionStats();

        this.impressionObserver.unobserve(this.refs.topsite_impression_wrapper);
      }
    };

    const options = {
      threshold: INTERSECTION_RATIO
    };
    this.impressionObserver = new props.IntersectionObserver(this._handleIntersect, options);
    this.impressionObserver.observe(this.refs.topsite_impression_wrapper);
  }

  componentDidMount() {
    if (this.props.tile) {
      this.setImpressionObserverOrAddListener();
    }
  }

  componentWillUnmount() {
    if (this._handleIntersect && this.impressionObserver) {
      this.impressionObserver.unobserve(this.refs.topsite_impression_wrapper);
    }

    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      ref: "topsite_impression_wrapper",
      className: "topsite-impression-observer"
    }, this.props.children);
  }

}
TopSiteImpressionWrapper.defaultProps = {
  IntersectionObserver: global.IntersectionObserver,
  document: global.document,
  tile: null
};
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 60 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "HorizontalRule", function() { return HorizontalRule; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class HorizontalRule extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("hr", {
      className: "ds-hr"
    });
  }

}

/***/ }),
/* 61 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Topic", function() { return Topic; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Navigation", function() { return Navigation; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var _SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(26);
/* harmony import */ var content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(30);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




class Topic extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "CLICK",
        source: "POPULAR_TOPICS",
        action_position: 0,
        value: {
          topic: event.target.text.toLowerCase().replace(` `, `-`)
        }
      }));
    }
  }

  render() {
    const {
      url,
      name
    } = this.props;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_2__["SafeAnchor"], {
      onLinkClick: this.onLinkClick,
      className: this.props.className,
      url: url
    }, name);
  }

}
class Navigation extends react__WEBPACK_IMPORTED_MODULE_1___default.a.PureComponent {
  render() {
    const links = this.props.links || [];
    const alignment = this.props.alignment || "centered";
    const header = this.props.header || {};
    const english = this.props.locale.startsWith("en-");
    const privacyNotice = this.props.privacyNoticeURL || {};
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("div", {
      className: `ds-navigation ds-navigation-${alignment}`
    }, header.title && english ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_3__["FluentOrText"], {
      message: header.title
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      className: "ds-navigation-header"
    })) : null, english ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("ul", null, links && links.map(t => /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("li", {
      key: t.name
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(Topic, {
      url: t.url,
      name: t.name,
      dispatch: this.props.dispatch
    })))) : null, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(_SafeAnchor_SafeAnchor__WEBPACK_IMPORTED_MODULE_2__["SafeAnchor"], {
      onLinkClick: this.onLinkClick,
      className: this.props.className,
      url: privacyNotice.url
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement(content_src_components_FluentOrText_FluentOrText__WEBPACK_IMPORTED_MODULE_3__["FluentOrText"], {
      message: privacyNotice.title
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_1___default.a.createElement("span", {
      className: "ds-navigation-privacy"
    }))));
  }

}

/***/ }),
/* 62 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SectionTitle", function() { return SectionTitle; });
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_0__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SectionTitle extends react__WEBPACK_IMPORTED_MODULE_0___default.a.PureComponent {
  render() {
    const {
      header: {
        title,
        subtitle
      }
    } = this.props;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "ds-section-title"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "title"
    }, title), subtitle ? /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_0___default.a.createElement("div", {
      className: "subtitle"
    }, subtitle) : null);
  }

}

/***/ }),
/* 63 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "selectLayoutRender", function() { return selectLayoutRender; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const selectLayoutRender = ({
  state = {},
  prefs = {},
  locale = ""
}) => {
  const {
    layout,
    feeds,
    spocs
  } = state;
  let spocIndexPlacementMap = {};
  /* This function fills spoc positions on a per placement basis with available spocs.
   * It does this by looping through each position for a placement and replacing a rec with a spoc.
   * If it runs out of spocs or positions, it stops.
   * If it sees the same placement again, it remembers the previous spoc index, and continues.
   * If it sees a blocked spoc, it skips that position leaving in a regular story.
   */

  function fillSpocPositionsForPlacement(data, spocsConfig, spocsData, placementName) {
    if (!spocIndexPlacementMap[placementName] && spocIndexPlacementMap[placementName] !== 0) {
      spocIndexPlacementMap[placementName] = 0;
    }

    const results = [...data];

    for (let position of spocsConfig.positions) {
      const spoc = spocsData[spocIndexPlacementMap[placementName]]; // If there are no spocs left, we can stop filling positions.

      if (!spoc) {
        break;
      } // A placement could be used in two sections.
      // In these cases, we want to maintain the index of the previous section.
      // If we didn't do this, it might duplicate spocs.


      spocIndexPlacementMap[placementName]++; // A spoc that's blocked is removed from the source for subsequent newtab loads.
      // If we have a spoc in the source that's blocked, it means it was *just* blocked,
      // and in this case, we skip this position, and show a regular spoc instead.

      if (!spocs.blocked.includes(spoc.url)) {
        results.splice(position.index, 0, spoc);
      }
    }

    return results;
  }

  const positions = {};
  const DS_COMPONENTS = ["Message", "TextPromo", "SectionTitle", "Signup", "Navigation", "CardGrid", "CollectionCardGrid", "Hero", "HorizontalRule", "List"];
  const filterArray = [];

  if (!prefs["feeds.topsites"]) {
    filterArray.push("TopSites");
  }

  const pocketEnabled = prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"];

  if (!pocketEnabled) {
    filterArray.push(...DS_COMPONENTS);
  }

  const placeholderComponent = component => {
    if (!component.feed) {
      // TODO we now need a placeholder for topsites and textPromo.
      return { ...component,
        data: {
          spocs: []
        }
      };
    }

    const data = {
      recommendations: []
    };
    let items = 0;

    if (component.properties && component.properties.items) {
      items = component.properties.items;
    }

    for (let i = 0; i < items; i++) {
      data.recommendations.push({
        placeholder: true
      });
    }

    return { ...component,
      data
    };
  }; // TODO update devtools to show placements


  const handleSpocs = (data, component) => {
    let result = [...data]; // Do we ever expect to possibly have a spoc.

    if (component.spocs && component.spocs.positions && component.spocs.positions.length) {
      const placement = component.placement || {};
      const placementName = placement.name || "spocs";
      const spocsData = spocs.data[placementName]; // We expect a spoc, spocs are loaded, and the server returned spocs.

      if (spocs.loaded && spocsData && spocsData.items && spocsData.items.length) {
        result = fillSpocPositionsForPlacement(result, component.spocs, spocsData.items, placementName);
      }
    }

    return result;
  };

  const handleComponent = component => {
    if (component.spocs && component.spocs.positions && component.spocs.positions.length) {
      const placement = component.placement || {};
      const placementName = placement.name || "spocs";
      const spocsData = spocs.data[placementName];

      if (spocs.loaded && spocsData && spocsData.items && spocsData.items.length) {
        return { ...component,
          data: {
            spocs: spocsData.items.filter(spoc => spoc && !spocs.blocked.includes(spoc.url)).map((spoc, index) => ({ ...spoc,
              pos: index
            }))
          }
        };
      }
    }

    return { ...component,
      data: {
        spocs: []
      }
    };
  };

  const handleComponentWithFeed = component => {
    positions[component.type] = positions[component.type] || 0;
    let data = {
      recommendations: []
    };
    const feed = feeds.data[component.feed.url];

    if (feed && feed.data) {
      data = { ...feed.data,
        recommendations: [...(feed.data.recommendations || [])]
      };
    }

    if (component && component.properties && component.properties.offset) {
      data = { ...data,
        recommendations: data.recommendations.slice(component.properties.offset)
      };
    }

    data = { ...data,
      recommendations: handleSpocs(data.recommendations, component)
    };
    let items = 0;

    if (component.properties && component.properties.items) {
      items = Math.min(component.properties.items, data.recommendations.length);
    } // loop through a component items
    // Store the items position sequentially for multiple components of the same type.
    // Example: A second card grid starts pos offset from the last card grid.


    for (let i = 0; i < items; i++) {
      data.recommendations[i] = { ...data.recommendations[i],
        pos: positions[component.type]++
      };
    }

    return { ...component,
      data
    };
  };

  const renderLayout = () => {
    const renderedLayoutArray = [];

    for (const row of layout.filter(r => r.components.filter(c => !filterArray.includes(c.type)).length)) {
      let components = [];
      renderedLayoutArray.push({ ...row,
        components
      });

      for (const component of row.components.filter(c => !filterArray.includes(c.type))) {
        const spocsConfig = component.spocs;

        if (spocsConfig || component.feed) {
          // TODO make sure this still works for different loading cases.
          if (component.feed && !feeds.data[component.feed.url] || spocsConfig && spocsConfig.positions && spocsConfig.positions.length && !spocs.loaded) {
            components.push(placeholderComponent(component));
            return renderedLayoutArray;
          }

          if (component.feed) {
            components.push(handleComponentWithFeed(component));
          } else {
            components.push(handleComponent(component));
          }
        } else {
          components.push(component);
        }
      }
    }

    return renderedLayoutArray;
  };

  const layoutRender = renderLayout();
  return {
    layoutRender
  };
};

/***/ }),
/* 64 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_TopSites", function() { return _TopSites; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSites", function() { return TopSites; });
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(55);
/* harmony import */ var common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(73);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




class _TopSites extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  // Find a SPOC that doesn't already exist in User's TopSites
  getFirstAvailableSpoc(topSites, data) {
    const {
      spocs
    } = data;

    if (!spocs || spocs.length === 0) {
      return null;
    }

    const userTopSites = new Set(topSites.map(topSite => topSite && topSite.url)); // We "clean urls" with http in TopSiteForm.jsx
    // Spoc domains are in the format 'sponsorname.com'

    return spocs.find(spoc => !userTopSites.has(spoc.url) && !userTopSites.has(`http://${spoc.domain}`) && !userTopSites.has(`https://${spoc.domain}`) && !userTopSites.has(`http://www.${spoc.domain}`) && !userTopSites.has(`https://www.${spoc.domain}`));
  } // Find the first empty or unpinned index we can place the SPOC in.
  // Return -1 if no available index and we should push it at the end.


  getFirstAvailableIndex(topSites, promoAlignment) {
    if (promoAlignment === "left") {
      return topSites.findIndex(topSite => !topSite || !topSite.isPinned);
    } // The row isn't full so we can push it to the end of the row.


    if (topSites.length < common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_MAX_SITES_PER_ROW"]) {
      return -1;
    } // If the row is full, we can check the row first for unpinned topsites to replace.
    // Else we can check after the row. This behavior is how unpinned topsites move while drag and drop.


    let endOfRow = common_Reducers_jsm__WEBPACK_IMPORTED_MODULE_2__["TOP_SITES_MAX_SITES_PER_ROW"] - 1;

    for (let i = endOfRow; i >= 0; i--) {
      if (!topSites[i] || !topSites[i].isPinned) {
        return i;
      }
    }

    for (let i = endOfRow + 1; i < topSites.length; i++) {
      if (!topSites[i] || !topSites[i].isPinned) {
        return i;
      }
    }

    return -1;
  }

  insertSpocContent(TopSites, data, promoAlignment) {
    if (!TopSites.rows || TopSites.rows.length === 0 || !data.spocs || data.spocs.length === 0) {
      return null;
    }

    let topSites = [...TopSites.rows];
    const topSiteSpoc = this.getFirstAvailableSpoc(topSites, data);

    if (!topSiteSpoc) {
      return null;
    }

    const link = {
      customScreenshotURL: topSiteSpoc.image_src,
      type: "SPOC",
      label: topSiteSpoc.sponsor,
      title: topSiteSpoc.sponsor,
      url: topSiteSpoc.url,
      flightId: topSiteSpoc.flight_id,
      id: topSiteSpoc.id,
      guid: topSiteSpoc.id,
      shim: topSiteSpoc.shim,
      // For now we are assuming position based on intended position.
      // Actual position can shift based on other content.
      // We also hard code left and right to be 0 and 7.
      // We send the intended postion in the ping.
      pos: promoAlignment === "left" ? 0 : 7
    };
    const firstAvailableIndex = this.getFirstAvailableIndex(topSites, promoAlignment);

    if (firstAvailableIndex === -1) {
      topSites.push(link);
    } else {
      // Normal insertion will not work since pinned topsites are in their correct index already
      // Similar logic is done to handle drag and drop with pinned topsites in TopSite.jsx
      let shiftedTopSite = topSites[firstAvailableIndex];
      let index = firstAvailableIndex + 1; // Shift unpinned topsites to the right by finding the next unpinned topsite to replace

      while (shiftedTopSite) {
        if (index === topSites.length) {
          topSites.push(shiftedTopSite);
          shiftedTopSite = null;
        } else if (topSites[index] && topSites[index].isPinned) {
          index += 1;
        } else {
          const nextTopSite = topSites[index];
          topSites[index] = shiftedTopSite;
          shiftedTopSite = nextTopSite;
          index += 1;
        }
      }

      topSites[firstAvailableIndex] = link;
    }

    return { ...TopSites,
      rows: topSites
    };
  }

  render() {
    const {
      header = {},
      data,
      promoAlignment,
      TopSites
    } = this.props;
    const TopSitesWithSpoc = TopSites && data && promoAlignment ? this.insertSpocContent(TopSites, data, promoAlignment) : null;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: `ds-top-sites ${TopSitesWithSpoc ? "top-sites-spoc" : ""}`
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement(content_src_components_TopSites_TopSites__WEBPACK_IMPORTED_MODULE_1__["TopSites"], {
      isFixed: true,
      title: header.title,
      TopSitesWithSpoc: TopSitesWithSpoc
    }));
  }

}
const TopSites = Object(react_redux__WEBPACK_IMPORTED_MODULE_0__["connect"])(state => ({
  TopSites: state.TopSites
}))(_TopSites);

/***/ }),
/* 65 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_Search", function() { return _Search; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Search", function() { return Search; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(react_redux__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var content_src_lib_constants__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(13);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(8);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_3__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ContentSearchUIController, ContentSearchHandoffUIController */


function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }





class _Search extends react__WEBPACK_IMPORTED_MODULE_3___default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onSearchClick = this.onSearchClick.bind(this);
    this.onSearchHandoffClick = this.onSearchHandoffClick.bind(this);
    this.onSearchHandoffPaste = this.onSearchHandoffPaste.bind(this);
    this.onSearchHandoffDrop = this.onSearchHandoffDrop.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
    this.onInputMountHandoff = this.onInputMountHandoff.bind(this);
    this.onSearchHandoffButtonMount = this.onSearchHandoffButtonMount.bind(this);
  }

  handleEvent(event) {
    // Also track search events with our own telemetry
    if (event.detail.type === "Search") {
      this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
        event: "SEARCH"
      }));
    }
  }

  onSearchClick(event) {
    window.gContentSearchController.search(event);
  }

  doSearchHandoff(text) {
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].OnlyToMain({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].HANDOFF_SEARCH_TO_AWESOMEBAR,
      data: {
        text
      }
    }));
    this.props.dispatch({
      type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].FAKE_FOCUS_SEARCH
    });
    this.props.dispatch(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].UserEvent({
      event: "SEARCH_HANDOFF"
    }));

    if (text) {
      this.props.dispatch({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].DISABLE_SEARCH
      });
    }
  }

  onSearchHandoffClick(event) {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked, we style
    // the button as if it was a focused search box and show a fake cursor but
    // really focus the awesomebar without the focus styles ("hidden focus").
    event.preventDefault();
    this.doSearchHandoff();
  }

  onSearchHandoffPaste(event) {
    event.preventDefault();
    this.doSearchHandoff(event.clipboardData.getData("Text"));
  }

  onSearchHandoffDrop(event) {
    event.preventDefault();
    let text = event.dataTransfer.getData("text");

    if (text) {
      this.doSearchHandoff(text);
    }
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
      const healthReportKey = content_src_lib_constants__WEBPACK_IMPORTED_MODULE_2__["IS_NEWTAB"] ? "newtab" : "abouthome"; // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)

      const searchSource = content_src_lib_constants__WEBPACK_IMPORTED_MODULE_2__["IS_NEWTAB"] ? "newtab" : "homepage"; // gContentSearchController needs to exist as a global so that tests for
      // the existing about:home can find it; and so it allows these tests to pass.
      // In the future, when activity stream is default about:home, this can be renamed

      window.gContentSearchController = new ContentSearchUIController(input, input.parentNode, healthReportKey, searchSource);
      addEventListener("ContentSearchClient", this);
    } else {
      window.gContentSearchController = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  onInputMountHandoff(input) {
    if (input) {
      // The handoff UI controller helps us set the search icon and reacts to
      // changes to default engine to keep everything in sync.
      this._handoffSearchController = new ContentSearchHandoffUIController();
    }
  }

  getDefaultEngineName() {
    // _handoffSearchController will manage engine names once it is initialized.
    return this.props.Prefs.values["urlbar.placeholderName"];
  }

  getHandoffInputL10nAttributes() {
    let defaultEngineName = this.getDefaultEngineName();
    return defaultEngineName ? {
      "data-l10n-id": "newtab-search-box-handoff-input",
      "data-l10n-args": `{"engine": "${defaultEngineName}"}`
    } : {
      "data-l10n-id": "newtab-search-box-handoff-input-no-engine"
    };
  }

  getHandoffTextL10nAttributes() {
    let defaultEngineName = this.getDefaultEngineName();
    return defaultEngineName ? {
      "data-l10n-id": "newtab-search-box-handoff-text",
      "data-l10n-args": `{"engine": "${defaultEngineName}"}`
    } : {
      "data-l10n-id": "newtab-search-box-handoff-text-no-engine"
    };
  }

  onSearchHandoffButtonMount(button) {
    // Keep a reference to the button for use during "paste" event handling.
    this._searchHandoffButton = button;
  }
  /*
   * Do not change the ID on the input field, as legacy newtab code
   * specifically looks for the id 'newtab-search-text' on input fields
   * in order to execute searches in various tests
   */


  render() {
    const wrapperClassName = ["search-wrapper", this.props.disable && "search-disabled", this.props.fakeFocus && "fake-focus"].filter(v => v).join(" ");
    const isNewNewtabExperienceEnabled = this.props.Prefs.values.featureConfig.newNewtabExperienceEnabled;
    return /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: wrapperClassName
    }, this.props.showLogo && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "logo-and-wordmark"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "logo"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "wordmark"
    })), !this.props.handoffEnabled && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "search-inner-wrapper"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      id: "newtab-search-text",
      "data-l10n-id": isNewNewtabExperienceEnabled ? "newtab-search-box-input" : "newtab-search-box-search-the-web-input",
      maxLength: "256",
      ref: this.onInputMount,
      type: "search"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", {
      id: "searchSubmit",
      className: "search-button",
      "data-l10n-id": "newtab-search-box-search-button",
      onClick: this.onSearchClick
    })), this.props.handoffEnabled && /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "search-inner-wrapper"
    }, /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("button", _extends({
      className: "search-handoff-button"
    }, this.getHandoffInputL10nAttributes(), {
      ref: this.onSearchHandoffButtonMount,
      onClick: this.onSearchHandoffClick,
      tabIndex: "-1"
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", _extends({
      className: "fake-textbox"
    }, this.getHandoffTextL10nAttributes())), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("input", {
      type: "search",
      className: "fake-editable",
      tabIndex: "-1",
      "aria-hidden": "true",
      onDrop: this.onSearchHandoffDrop,
      onPaste: this.onSearchHandoffPaste,
      ref: this.onInputMountHandoff
    }), /*#__PURE__*/react__WEBPACK_IMPORTED_MODULE_3___default.a.createElement("div", {
      className: "fake-caret"
    }))));
  }

}
const Search = Object(react_redux__WEBPACK_IMPORTED_MODULE_1__["connect"])(state => ({
  Prefs: state.Prefs
}))(_Search);

/***/ }),
/* 66 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* WEBPACK VAR INJECTION */(function(global) {/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "DetectUserSessionStart", function() { return DetectUserSessionStart; });
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var content_src_lib_perf_service__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(51);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
class DetectUserSessionStart {
  constructor(store, options = {}) {
    this._store = store; // Overrides for testing

    this.document = options.document || global.document;
    this._perfService = options.perfService || content_src_lib_perf_service__WEBPACK_IMPORTED_MODULE_1__["perfService"];
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
        data: {
          visibility_event_rcvd_ts
        }
      }));
    } catch (ex) {// If this failed, it's likely because the `privacy.resistFingerprinting`
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
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 67 */
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
/* harmony import */ var common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony import */ var redux__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(68);
/* harmony import */ var redux__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(redux__WEBPACK_IMPORTED_MODULE_1__);
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */


const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
const EARLY_QUEUED_ACTIONS = [common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].SAVE_SESSION_PERF_DATA];
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
      return { ...prevState,
        ...action.data
      };
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

const rehydrationMiddleware = ({
  getState
}) => {
  // NB: The parameter here is MiddlewareAPI which looks like a Store and shares
  // the same getState, so attached properties are accessible from the store.
  getState.didRehydrate = false;
  getState.didRequestInitialState = false;
  return next => action => {
    if (getState.didRehydrate || window.__FROM_STARTUP_CACHE__) {
      // Startup messages can be safely ignored by the about:home document
      // stored in the startup cache.
      if (window.__FROM_STARTUP_CACHE__ && action.meta && action.meta.isStartup) {
        return null;
      }

      return next(action);
    }

    const isMergeStoreAction = action.type === MERGE_STORE_ACTION;
    const isRehydrationRequest = action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST;

    if (isRehydrationRequest) {
      getState.didRequestInitialState = true;
      return next(action);
    }

    if (isMergeStoreAction) {
      getState.didRehydrate = true;
      return next(action);
    } // If init happened after our request was made, we need to re-request


    if (getState.didRequestInitialState && action.type === common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].INIT) {
      return next(common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionCreators"].AlsoToMain({
        type: common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionTypes"].NEW_TAB_STATE_REQUEST
      }));
    }

    if (common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isBroadcastToContent(action) || common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isSendToOneContent(action) || common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isSendToPreloaded(action)) {
      // Note that actions received before didRehydrate will not be dispatched
      // because this could negatively affect preloading and the the state
      // will be replaced by rehydration anyway.
      return null;
    }

    return next(action);
  };
};
/**
 * This middleware queues up all the EARLY_QUEUED_ACTIONS until it receives
 * the first action from main. This is useful for those actions for main which
 * require higher reliability, i.e. the action will not be lost in the case
 * that it gets sent before the main is ready to receive it. Conversely, any
 * actions allowed early are accepted to be ignorable or re-sendable.
 */

const queueEarlyMessageMiddleware = ({
  getState
}) => {
  // NB: The parameter here is MiddlewareAPI which looks like a Store and shares
  // the same getState, so attached properties are accessible from the store.
  getState.earlyActionQueue = [];
  getState.receivedFromMain = false;
  return next => action => {
    if (getState.receivedFromMain) {
      next(action);
    } else if (common_Actions_jsm__WEBPACK_IMPORTED_MODULE_0__["actionUtils"].isFromMain(action)) {
      next(action);
      getState.receivedFromMain = true; // Sending out all the early actions as main is ready now

      getState.earlyActionQueue.forEach(next);
      getState.earlyActionQueue.length = 0;
    } else if (EARLY_QUEUED_ACTIONS.includes(action.type)) {
      getState.earlyActionQueue.push(action);
    } else {
      // Let any other type of action go through
      next(action);
    }
  };
};
/**
 * initStore - Create a store and listen for incoming actions
 *
 * @param  {object} reducers An object containing Redux reducers
 * @param  {object} intialState (optional) The initial state of the store, if desired
 * @return {object}          A redux store
 */

function initStore(reducers, initialState) {
  const store = Object(redux__WEBPACK_IMPORTED_MODULE_1__["createStore"])(mergeStateReducer(Object(redux__WEBPACK_IMPORTED_MODULE_1__["combineReducers"])(reducers)), initialState, global.RPMAddMessageListener && Object(redux__WEBPACK_IMPORTED_MODULE_1__["applyMiddleware"])(queueEarlyMessageMiddleware, rehydrationMiddleware, messageMiddleware));

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
/* WEBPACK VAR INJECTION */}.call(this, __webpack_require__(3)))

/***/ }),
/* 68 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 69 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(8);

// EXTERNAL MODULE: external "PropTypes"
var external_PropTypes_ = __webpack_require__(12);
var external_PropTypes_default = /*#__PURE__*/__webpack_require__.n(external_PropTypes_);

// CONCATENATED MODULE: ./node_modules/fluent-sequence/src/map_sync.js
/*
 * Synchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param {Iterable} iterable
 * @param {string|Array<string>} ids
 * @returns {FluentBundle|Array<FluentBundle>}
 */
function mapBundleSync(iterable, ids) {
  if (!Array.isArray(ids)) {
    return getBundleForId(iterable, ids);
  }

  return ids.map(
    id => getBundleForId(iterable, id)
  );
}

/*
 * Find the best `FluentBundle` with the translation for `id`.
 */
function getBundleForId(iterable, id) {
  for (const bundle of iterable) {
    if (bundle.hasMessage(id)) {
      return bundle;
    }
  }

  return null;
}

// CONCATENATED MODULE: ./node_modules/fluent-sequence/src/map_async.js
/*
 * Asynchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param {AsyncIterable} iterable
 * @param {string|Array<string>} ids
 * @returns {Promise<FluentBundle|Array<FluentBundle>>}
 */
async function mapBundleAsync(iterable, ids) {
  if (!Array.isArray(ids)) {
    for await (const bundle of iterable) {
      if (bundle.hasMessage(ids)) {
        return bundle;
      }
    }
  }

  let remainingCount = ids.length;
  const foundBundles = new Array(remainingCount).fill(null);

  for await (const bundle of iterable) {
    for (const [index, id] of ids.entries()) {
      if (!foundBundles[index] && bundle.hasMessage(id)) {
        foundBundles[index] = bundle;
        remainingCount--;
      }

      // Return early when all ids have been mapped to contexts.
      if (remainingCount === 0) {
        return foundBundles;
      }
    }
  }

  return foundBundles;
}

// CONCATENATED MODULE: ./node_modules/fluent-sequence/src/index.js
/*
 * @module fluent-sequence
 * @overview Manage ordered sequences of FluentBundles.
 */




// CONCATENATED MODULE: ./node_modules/cached-iterable/src/cached_iterable.mjs
/*
 * Base CachedIterable class.
 */
class CachedIterable extends Array {
    /**
     * Create a `CachedIterable` instance from an iterable or, if another
     * instance of `CachedIterable` is passed, return it without any
     * modifications.
     *
     * @param {Iterable} iterable
     * @returns {CachedIterable}
     */
    static from(iterable) {
        if (iterable instanceof this) {
            return iterable;
        }

        return new this(iterable);
    }
}

// CONCATENATED MODULE: ./node_modules/cached-iterable/src/cached_sync_iterable.mjs


/*
 * CachedSyncIterable caches the elements yielded by an iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class cached_sync_iterable_CachedSyncIterable extends CachedIterable {
    /**
     * Create an `CachedSyncIterable` instance.
     *
     * @param {Iterable} iterable
     * @returns {CachedSyncIterable}
     */
    constructor(iterable) {
        super();

        if (Symbol.iterator in Object(iterable)) {
            this.iterator = iterable[Symbol.iterator]();
        } else {
            throw new TypeError("Argument must implement the iteration protocol.");
        }
    }

    [Symbol.iterator]() {
        const cached = this;
        let cur = 0;

        return {
            next() {
                if (cached.length <= cur) {
                    cached.push(cached.iterator.next());
                }
                return cached[cur++];
            }
        };
    }

    /**
     * This method allows user to consume the next element from the iterator
     * into the cache.
     *
     * @param {number} count - number of elements to consume
     */
    touchNext(count = 1) {
        let idx = 0;
        while (idx++ < count) {
            const last = this[this.length - 1];
            if (last && last.done) {
                break;
            }
            this.push(this.iterator.next());
        }
        // Return the last cached {value, done} object to allow the calling
        // code to decide if it needs to call touchNext again.
        return this[this.length - 1];
    }
}

// CONCATENATED MODULE: ./node_modules/cached-iterable/src/cached_async_iterable.mjs


/*
 * CachedAsyncIterable caches the elements yielded by an async iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class cached_async_iterable_CachedAsyncIterable extends CachedIterable {
    /**
     * Create an `CachedAsyncIterable` instance.
     *
     * @param {Iterable} iterable
     * @returns {CachedAsyncIterable}
     */
    constructor(iterable) {
        super();

        if (Symbol.asyncIterator in Object(iterable)) {
            this.iterator = iterable[Symbol.asyncIterator]();
        } else if (Symbol.iterator in Object(iterable)) {
            this.iterator = iterable[Symbol.iterator]();
        } else {
            throw new TypeError("Argument must implement the iteration protocol.");
        }
    }

    /**
     * Synchronous iterator over the cached elements.
     *
     * Return a generator object implementing the iterator protocol over the
     * cached elements of the original (async or sync) iterable.
     */
    [Symbol.iterator]() {
        const cached = this;
        let cur = 0;

        return {
            next() {
                if (cached.length === cur) {
                    return {value: undefined, done: true};
                }
                return cached[cur++];
            }
        };
    }

    /**
     * Asynchronous iterator caching the yielded elements.
     *
     * Elements yielded by the original iterable will be cached and available
     * synchronously. Returns an async generator object implementing the
     * iterator protocol over the elements of the original (async or sync)
     * iterable.
     */
    [Symbol.asyncIterator]() {
        const cached = this;
        let cur = 0;

        return {
            async next() {
                if (cached.length <= cur) {
                    cached.push(await cached.iterator.next());
                }
                return cached[cur++];
            }
        };
    }

    /**
     * This method allows user to consume the next element from the iterator
     * into the cache.
     *
     * @param {number} count - number of elements to consume
     */
    async touchNext(count = 1) {
        let idx = 0;
        while (idx++ < count) {
            const last = this[this.length - 1];
            if (last && last.done) {
                break;
            }
            this.push(await this.iterator.next());
        }
        // Return the last cached {value, done} object to allow the calling
        // code to decide if it needs to call touchNext again.
        return this[this.length - 1];
    }
}

// CONCATENATED MODULE: ./node_modules/cached-iterable/src/index.mjs



// CONCATENATED MODULE: ./node_modules/fluent-react/src/localization.js


/*
 * `ReactLocalization` handles translation formatting and fallback.
 *
 * The current negotiated fallback chain of languages is stored in the
 * `ReactLocalization` instance in form of an iterable of `FluentBundle`
 * instances.  This iterable is used to find the best existing translation for
 * a given identifier.
 *
 * `Localized` components must subscribe to the changes of the
 * `ReactLocalization`'s fallback chain.  When the fallback chain changes (the
 * `bundles` iterable is set anew), all subscribed compontent must relocalize.
 *
 * The `ReactLocalization` class instances are exposed to `Localized` elements
 * via the `LocalizationProvider` component.
 */

class localization_ReactLocalization {
  constructor(bundles) {
    this.bundles = cached_sync_iterable_CachedSyncIterable.from(bundles);
    this.subs = new Set();
  }
  /*
   * Subscribe a `Localized` component to changes of `bundles`.
   */


  subscribe(comp) {
    this.subs.add(comp);
  }
  /*
   * Unsubscribe a `Localized` component from `bundles` changes.
   */


  unsubscribe(comp) {
    this.subs.delete(comp);
  }
  /*
   * Set a new `bundles` iterable and trigger the retranslation.
   */


  setBundles(bundles) {
    this.bundles = cached_sync_iterable_CachedSyncIterable.from(bundles); // Update all subscribed Localized components.

    this.subs.forEach(comp => comp.relocalize());
  }

  getBundle(id) {
    return mapBundleSync(this.bundles, id);
  }
  /*
   * Find a translation by `id` and format it to a string using `args`.
   */


  getString(id, args, fallback) {
    const bundle = this.getBundle(id);

    if (bundle === null) {
      return fallback || id;
    }

    const msg = bundle.getMessage(id);
    return bundle.format(msg, args);
  }

}
function isReactLocalization(props, propName) {
  const prop = props[propName];

  if (prop instanceof localization_ReactLocalization) {
    return null;
  }

  return new Error(`The ${propName} context field must be an instance of ReactLocalization.`);
}
// CONCATENATED MODULE: ./node_modules/fluent-react/src/markup.js
/* eslint-env browser */
let cachedParseMarkup; // We use a function creator to make the reference to `document` lazy. At the
// same time, it's eager enough to throw in <LocalizationProvider> as soon as
// it's first mounted which reduces the risk of this error making it to the
// runtime without developers noticing it in development.

function createParseMarkup() {
  if (typeof document === "undefined") {
    // We can't use <template> to sanitize translations.
    throw new Error("`document` is undefined. Without it, translations cannot " + "be safely sanitized. Consult the documentation at " + "https://github.com/projectfluent/fluent.js/wiki/React-Overlays.");
  }

  if (!cachedParseMarkup) {
    const template = document.createElement("template");

    cachedParseMarkup = function parseMarkup(str) {
      template.innerHTML = str;
      return Array.from(template.content.childNodes);
    };
  }

  return cachedParseMarkup;
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
 *     <LocalizationProvider bundles={…}>
 *         …
 *     </LocalizationProvider>
 *
 * The `LocalizationProvider` component takes one prop: `bundles`.  It should
 * be an iterable of `FluentBundle` instances in order of the user's
 * preferred languages.  The `FluentBundle` instances will be used by
 * `ReactLocalization` to format translations.  If a translation is missing in
 * one instance, `ReactLocalization` will fall back to the next one.
 */

class provider_LocalizationProvider extends external_React_["Component"] {
  constructor(props) {
    super(props);
    const {
      bundles,
      parseMarkup
    } = props;

    if (bundles === undefined) {
      throw new Error("LocalizationProvider must receive the bundles prop.");
    }

    if (!bundles[Symbol.iterator]) {
      throw new Error("The bundles prop must be an iterable.");
    }

    this.l10n = new localization_ReactLocalization(bundles);
    this.parseMarkup = parseMarkup || createParseMarkup();
  }

  getChildContext() {
    return {
      l10n: this.l10n,
      parseMarkup: this.parseMarkup
    };
  }

  componentWillReceiveProps(next) {
    const {
      bundles
    } = next;

    if (bundles !== this.props.bundles) {
      this.l10n.setBundles(bundles);
    }
  }

  render() {
    return external_React_["Children"].only(this.props.children);
  }

}
provider_LocalizationProvider.childContextTypes = {
  l10n: isReactLocalization,
  parseMarkup: external_PropTypes_default.a.func
};
provider_LocalizationProvider.propTypes = {
  children: external_PropTypes_default.a.element.isRequired,
  bundles: isIterable,
  parseMarkup: external_PropTypes_default.a.func
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
      const {
        l10n
      } = this.context;

      if (l10n) {
        l10n.subscribe(this);
      }
    }

    componentWillUnmount() {
      const {
        l10n
      } = this.context;

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
      const {
        l10n
      } = this.context;

      if (!l10n) {
        return fallback || id;
      }

      return l10n.getString(id, args, fallback);
    }

    render() {
      return /*#__PURE__*/Object(external_React_["createElement"])(Inner, Object.assign( // getString needs to be re-bound on updates to trigger a re-render
      {
        getString: (...args) => this.getString(...args)
      }, this.props));
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
  wbr: true // NOTE: menuitem's close tag should be omitted, but that causes problems.

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

var voidElementTags = {
  menuitem: true,
  ...vendor_omittedCloseTags
};
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
    } else if ( /*#__PURE__*/Object(external_React_["isValidElement"])(propval)) {
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
    const {
      l10n
    } = this.context;

    if (l10n) {
      l10n.subscribe(this);
    }
  }

  componentWillUnmount() {
    const {
      l10n
    } = this.context;

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
    const {
      l10n,
      parseMarkup
    } = this.context;
    const {
      id,
      attrs,
      children: elem = null
    } = this.props; // Validate that the child element isn't an array

    if (Array.isArray(elem)) {
      throw new Error("<Localized/> expected to receive a single " + "React node child");
    }

    if (!l10n) {
      // Use the wrapped component as fallback.
      return elem;
    }

    const bundle = l10n.getBundle(id);

    if (bundle === null) {
      // Use the wrapped component as fallback.
      return elem;
    }

    const msg = bundle.getMessage(id);
    const [args, elems] = toArguments(this.props);
    const messageValue = bundle.format(msg, args); // Check if the fallback is a valid element -- if not then it's not
    // markup (e.g. nothing or a fallback string) so just use the
    // formatted message value

    if (! /*#__PURE__*/Object(external_React_["isValidElement"])(elem)) {
      return messageValue;
    } // The default is to forbid all message attributes. If the attrs prop exists
    // on the Localized instance, only set message attributes which have been
    // explicitly allowed by the developer.


    if (attrs && msg.attrs) {
      var localizedProps = {};

      for (const [name, allowed] of Object.entries(attrs)) {
        if (allowed && msg.attrs.hasOwnProperty(name)) {
          localizedProps[name] = bundle.format(msg.attrs[name], args);
        }
      }
    } // If the wrapped component is a known void element, explicitly dismiss the
    // message value and do not pass it to cloneElement in order to avoid the
    // "void element tags must neither have `children` nor use
    // `dangerouslySetInnerHTML`" error.


    if (elem.type in vendor_voidElementTags) {
      return /*#__PURE__*/Object(external_React_["cloneElement"])(elem, localizedProps);
    } // If the message has a null value, we're only interested in its attributes.
    // Do not pass the null value to cloneElement as it would nuke all children
    // of the wrapped component.


    if (messageValue === null) {
      return /*#__PURE__*/Object(external_React_["cloneElement"])(elem, localizedProps);
    } // If the message value doesn't contain any markup nor any HTML entities,
    // insert it as the only child of the wrapped component.


    if (!reMarkup.test(messageValue)) {
      return /*#__PURE__*/Object(external_React_["cloneElement"])(elem, localizedProps, messageValue);
    } // If the message contains markup, parse it and try to match the children
    // found in the translation with the props passed to this Localized.


    const translationNodes = parseMarkup(messageValue);
    const translatedChildren = translationNodes.map(childNode => {
      if (childNode.nodeType === childNode.TEXT_NODE) {
        return childNode.textContent;
      } // If the child is not expected just take its textContent.


      if (!elems.hasOwnProperty(childNode.localName)) {
        return childNode.textContent;
      }

      const sourceChild = elems[childNode.localName]; // If the element passed as a prop to <Localized> is a known void element,
      // explicitly dismiss any textContent which might have accidentally been
      // defined in the translation to prevent the "void element tags must not
      // have children" error.

      if (sourceChild.type in vendor_voidElementTags) {
        return sourceChild;
      } // TODO Protect contents of elements wrapped in <Localized>
      // https://github.com/projectfluent/fluent.js/issues/184
      // TODO  Control localizable attributes on elements passed as props
      // https://github.com/projectfluent/fluent.js/issues/185


      return /*#__PURE__*/Object(external_React_["cloneElement"])(sourceChild, null, childNode.textContent);
    });
    return /*#__PURE__*/Object(external_React_["cloneElement"])(elem, localizedProps, ...translatedChildren);
  }

}
localized_Localized.contextTypes = {
  l10n: isReactLocalization,
  parseMarkup: external_PropTypes_default.a.func
};
localized_Localized.propTypes = {
  children: external_PropTypes_default.a.node
};
// CONCATENATED MODULE: ./node_modules/fluent-react/src/index.js
/* concated harmony reexport LocalizationProvider */__webpack_require__.d(__webpack_exports__, "LocalizationProvider", function() { return provider_LocalizationProvider; });
/* concated harmony reexport withLocalization */__webpack_require__.d(__webpack_exports__, "withLocalization", function() { return withLocalization; });
/* concated harmony reexport Localized */__webpack_require__.d(__webpack_exports__, "Localized", function() { return localized_Localized; });
/* concated harmony reexport ReactLocalization */__webpack_require__.d(__webpack_exports__, "ReactLocalization", function() { return localization_ReactLocalization; });
/* concated harmony reexport isReactLocalization */__webpack_require__.d(__webpack_exports__, "isReactLocalization", function() { return isReactLocalization; });
/*
 * @module fluent-react
 * @overview
 *

 * `fluent-react` provides React bindings for Fluent.  It takes advantage of
 * React's Components system and the virtual DOM.  Translations are exposed to
 * components via the provider pattern.
 *
 *     <LocalizationProvider bundles={…}>
 *         <Localized id="hello-world">
 *             <p>{'Hello, world!'}</p>
 *         </Localized>
 *     </LocalizationProvider>
 *
 * Consult the documentation of the `LocalizationProvider` and the `Localized`
 * components for more information.
 */





/***/ }),
/* 70 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(8);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// CONCATENATED MODULE: ./content-src/asrouter/components/Button/Button.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const ALLOWED_STYLE_TAGS = ["color", "backgroundColor"];
const Button = props => {
  const style = {}; // Add allowed style tags from props, e.g. props.color becomes style={color: props.color}

  for (const tag of ALLOWED_STYLE_TAGS) {
    if (typeof props[tag] !== "undefined") {
      style[tag] = props[tag];
    }
  } // remove border if bg is set to something custom


  if (style.backgroundColor) {
    style.border = "0";
  }

  return /*#__PURE__*/external_React_default.a.createElement("button", {
    onClick: props.onClick,
    className: props.className || "ASRouterButton secondary",
    style: style
  }, props.children);
};
// CONCATENATED MODULE: ./content-src/asrouter/components/ConditionalWrapper/ConditionalWrapper.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// lifted from https://gist.github.com/kitze/23d82bb9eb0baabfd03a6a720b1d637f
const ConditionalWrapper = ({
  condition,
  wrap,
  children
}) => condition && wrap ? wrap(children) : children;

/* harmony default export */ var ConditionalWrapper_ConditionalWrapper = (ConditionalWrapper);
// EXTERNAL MODULE: ./node_modules/fluent-react/src/index.js + 14 modules
var src = __webpack_require__(69);

// EXTERNAL MODULE: ./content-src/asrouter/rich-text-strings.js + 7 modules
var rich_text_strings = __webpack_require__(71);

// CONCATENATED MODULE: ./content-src/asrouter/template-utils.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
function safeURI(url) {
  if (!url) {
    return "";
  }

  const {
    protocol
  } = new URL(url);
  const isAllowed = ["http:", "https:", "data:", "resource:", "chrome:"].includes(protocol);

  if (!isAllowed) {
    console.warn(`The protocol ${protocol} is not allowed for template URLs.`); // eslint-disable-line no-console
  }

  return isAllowed ? url : "";
}
// CONCATENATED MODULE: ./content-src/asrouter/components/RichText/RichText.jsx
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



 // Elements allowed in snippet content

const ALLOWED_TAGS = {
  b: /*#__PURE__*/external_React_default.a.createElement("b", null),
  i: /*#__PURE__*/external_React_default.a.createElement("i", null),
  u: /*#__PURE__*/external_React_default.a.createElement("u", null),
  strong: /*#__PURE__*/external_React_default.a.createElement("strong", null),
  em: /*#__PURE__*/external_React_default.a.createElement("em", null),
  br: /*#__PURE__*/external_React_default.a.createElement("br", null)
};
/**
 * Transform an object (tag name: {url}) into (tag name: anchor) where the url
 * is used as href, in order to render links inside a Fluent.Localized component.
 */

function convertLinks(links, sendClick, doNotAutoBlock, openNewWindow = false) {
  if (links) {
    return Object.keys(links).reduce((acc, linkTag) => {
      const {
        action
      } = links[linkTag]; // Setting the value to false will not include the attribute in the anchor

      const url = action ? false : safeURI(links[linkTag].url);
      acc[linkTag] =
      /*#__PURE__*/
      // eslint was getting a false positive caused by the dynamic injection
      // of content.
      // eslint-disable-next-line jsx-a11y/anchor-has-content
      external_React_default.a.createElement("a", {
        href: url,
        target: openNewWindow ? "_blank" : "",
        "data-metric": links[linkTag].metric,
        "data-action": action,
        "data-args": links[linkTag].args,
        "data-do_not_autoblock": doNotAutoBlock,
        "data-entrypoint_name": links[linkTag].entrypoint_name,
        "data-entrypoint_value": links[linkTag].entrypoint_value,
        rel: "noreferrer",
        onClick: sendClick
      });
      return acc;
    }, {});
  }

  return null;
}
/**
 * Message wrapper used to sanitize markup and render HTML.
 */

function RichText(props) {
  if (!rich_text_strings["RICH_TEXT_KEYS"].includes(props.localization_id)) {
    throw new Error(`ASRouter: ${props.localization_id} is not a valid rich text property. If you want it to be processed, you need to add it to asrouter/rich-text-strings.js`);
  }

  return /*#__PURE__*/external_React_default.a.createElement(src["Localized"], _extends({
    id: props.localization_id
  }, ALLOWED_TAGS, props.customElements, convertLinks(props.links, props.sendClick, props.doNotAutoBlock, props.openNewWindow)), /*#__PURE__*/external_React_default.a.createElement("span", null, props.text));
}
// CONCATENATED MODULE: ./content-src/asrouter/components/SnippetBase/SnippetBase.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SnippetBase_SnippetBase extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
    this.onDismissClicked = this.onDismissClicked.bind(this);
    this.setBlockButtonRef = this.setBlockButtonRef.bind(this);
    this.onBlockButtonMouseEnter = this.onBlockButtonMouseEnter.bind(this);
    this.onBlockButtonMouseLeave = this.onBlockButtonMouseLeave.bind(this);
    this.state = {
      blockButtonHover: false
    };
  }

  componentDidMount() {
    if (this.blockButtonRef) {
      this.blockButtonRef.addEventListener("mouseenter", this.onBlockButtonMouseEnter);
      this.blockButtonRef.addEventListener("mouseleave", this.onBlockButtonMouseLeave);
    }
  }

  componentWillUnmount() {
    if (this.blockButtonRef) {
      this.blockButtonRef.removeEventListener("mouseenter", this.onBlockButtonMouseEnter);
      this.blockButtonRef.removeEventListener("mouseleave", this.onBlockButtonMouseLeave);
    }
  }

  setBlockButtonRef(element) {
    this.blockButtonRef = element;
  }

  onBlockButtonMouseEnter() {
    this.setState({
      blockButtonHover: true
    });
  }

  onBlockButtonMouseLeave() {
    this.setState({
      blockButtonHover: false
    });
  }

  onBlockClicked() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "BLOCK",
        id: this.props.UISurface
      });
    }

    this.props.onBlock();
  }

  onDismissClicked() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "DISMISS",
        id: this.props.UISurface
      });
    }

    this.props.onDismiss();
  }

  renderDismissButton() {
    if (this.props.footerDismiss) {
      return /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "footer"
      }, /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "footer-content"
      }, /*#__PURE__*/external_React_default.a.createElement("button", {
        className: "ASRouterButton secondary",
        onClick: this.onDismissClicked
      }, this.props.content.scene2_dismiss_button_text)));
    }

    const label = this.props.content.block_button_text || "Remove this";
    return /*#__PURE__*/external_React_default.a.createElement("button", {
      className: "blockButton",
      title: label,
      "aria-label": label,
      onClick: this.onBlockClicked,
      ref: this.setBlockButtonRef
    });
  }

  render() {
    const {
      props
    } = this;
    const {
      blockButtonHover
    } = this.state;
    const containerClassName = `SnippetBaseContainer${props.className ? ` ${props.className}` : ""}${blockButtonHover ? " active" : ""}`;
    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: containerClassName,
      style: this.props.textStyle
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "innerWrapper"
    }, props.children), this.renderDismissButton());
  }

}
// CONCATENATED MODULE: ./content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx
function SimpleSnippet_extends() { SimpleSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SimpleSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png"; // Alt text placeholder in case the prop from the server isn't available

const ICON_ALT_TEXT = "";
class SimpleSnippet_SimpleSnippet extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  onButtonClick() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        id: this.props.UISurface
      });
    }

    const {
      button_url,
      button_entrypoint_value,
      button_entrypoint_name
    } = this.props.content; // If button_url is defined handle it as OPEN_URL action

    const type = this.props.content.button_action || button_url && "OPEN_URL"; // Assign the snippet referral for the action

    const entrypoint = button_entrypoint_name ? new URLSearchParams([[button_entrypoint_name, button_entrypoint_value]]).toString() : button_entrypoint_value;
    this.props.onAction({
      type,
      data: {
        args: this.props.content.button_action_args || button_url,
        ...(entrypoint && {
          entrypoint
        })
      }
    });

    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
  }

  _shouldRenderButton() {
    return this.props.content.button_action || this.props.onButtonClick || this.props.content.button_url;
  }

  renderTitle() {
    const {
      title
    } = this.props.content;
    return title ? /*#__PURE__*/external_React_default.a.createElement("h3", {
      className: `title ${this._shouldRenderButton() ? "title-inline" : ""}`
    }, this.renderTitleIcon(), " ", title) : null;
  }

  renderTitleIcon() {
    const titleIconLight = safeURI(this.props.content.title_icon);
    const titleIconDark = safeURI(this.props.content.title_icon_dark_theme || this.props.content.title_icon);

    if (!titleIconLight) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement(external_React_default.a.Fragment, null, /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "titleIcon icon-light-theme",
      style: {
        backgroundImage: `url("${titleIconLight}")`
      }
    }), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "titleIcon icon-dark-theme",
      style: {
        backgroundImage: `url("${titleIconDark}")`
      }
    }));
  }

  renderButton() {
    const {
      props
    } = this;

    if (!this._shouldRenderButton()) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement(Button, {
      onClick: props.onButtonClick || this.onButtonClick,
      color: props.content.button_color,
      backgroundColor: props.content.button_background_color
    }, props.content.button_label);
  }

  renderText() {
    const {
      props
    } = this;
    return /*#__PURE__*/external_React_default.a.createElement(RichText, {
      text: props.content.text,
      customElements: this.props.customElements,
      localization_id: "text",
      links: props.content.links,
      sendClick: props.sendClick
    });
  }

  wrapSectionHeader(url) {
    return function (children) {
      return /*#__PURE__*/external_React_default.a.createElement("a", {
        href: url
      }, children);
    };
  }

  wrapSnippetContent(children) {
    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "innerContentWrapper"
    }, children);
  }

  renderSectionHeader() {
    const {
      props
    } = this; // an icon and text must be specified to render the section header

    if (props.content.section_title_icon && props.content.section_title_text) {
      const sectionTitleIconLight = safeURI(props.content.section_title_icon);
      const sectionTitleIconDark = safeURI(props.content.section_title_icon_dark_theme || props.content.section_title_icon);
      const sectionTitleURL = props.content.section_title_url;
      return /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "section-header"
      }, /*#__PURE__*/external_React_default.a.createElement("h3", {
        className: "section-title"
      }, /*#__PURE__*/external_React_default.a.createElement(ConditionalWrapper_ConditionalWrapper, {
        condition: sectionTitleURL,
        wrap: this.wrapSectionHeader(sectionTitleURL)
      }, /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "icon icon-small-spacer icon-light-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconLight}")`
        }
      }), /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "icon icon-small-spacer icon-dark-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconDark}")`
        }
      }), /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "section-title-text"
      }, props.content.section_title_text))));
    }

    return null;
  }

  render() {
    const {
      props
    } = this;
    const sectionHeader = this.renderSectionHeader();
    let className = "SimpleSnippet";

    if (props.className) {
      className += ` ${props.className}`;
    }

    if (props.content.tall) {
      className += " tall";
    }

    if (sectionHeader) {
      className += " has-section-header";
    }

    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "snippet-hover-wrapper"
    }, /*#__PURE__*/external_React_default.a.createElement(SnippetBase_SnippetBase, SimpleSnippet_extends({}, props, {
      className: className,
      textStyle: this.props.textStyle
    }), sectionHeader, /*#__PURE__*/external_React_default.a.createElement(ConditionalWrapper_ConditionalWrapper, {
      condition: sectionHeader,
      wrap: this.wrapSnippetContent
    }, /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(props.content.icon) || DEFAULT_ICON_PATH,
      className: "icon icon-light-theme",
      alt: props.content.icon_alt_text || ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(props.content.icon_dark_theme || props.content.icon) || DEFAULT_ICON_PATH,
      className: "icon icon-dark-theme",
      alt: props.content.icon_alt_text || ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default.a.createElement("div", null, this.renderTitle(), " ", /*#__PURE__*/external_React_default.a.createElement("p", {
      className: "body"
    }, this.renderText()), this.props.extraContent), /*#__PURE__*/external_React_default.a.createElement("div", null, this.renderButton()))));
  }

}
// CONCATENATED MODULE: ./content-src/asrouter/templates/EOYSnippet/EOYSnippet.jsx
function EOYSnippet_extends() { EOYSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return EOYSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class EOYSnippet_EOYSnippetBase extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.handleSubmit = this.handleSubmit.bind(this);
  }
  /**
   * setFrequencyValue - `frequency` form parameter value should be `monthly`
   *                     if `monthly-checkbox` is selected or `single` otherwise
   */


  setFrequencyValue() {
    const frequencyCheckbox = this.refs.form.querySelector("#monthly-checkbox");

    if (frequencyCheckbox.checked) {
      this.refs.form.querySelector("[name='frequency']").value = "monthly";
    }
  }

  handleSubmit(event) {
    event.preventDefault();
    this.props.sendClick(event);
    this.setFrequencyValue();

    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }

    this.refs.form.submit();
  }

  renderDonations() {
    const fieldNames = ["first", "second", "third", "fourth"];
    const numberFormat = new Intl.NumberFormat(this.props.content.locale || navigator.language, {
      style: "currency",
      currency: this.props.content.currency_code,
      minimumFractionDigits: 0
    }); // Default to `second` button

    const {
      selected_button
    } = this.props.content;
    const btnStyle = {
      color: this.props.content.button_color,
      backgroundColor: this.props.content.button_background_color
    };
    const donationURLParams = [];
    const paramsStartIndex = this.props.content.donation_form_url.indexOf("?");

    for (const entry of new URLSearchParams(this.props.content.donation_form_url.slice(paramsStartIndex)).entries()) {
      donationURLParams.push(entry);
    }

    return /*#__PURE__*/external_React_default.a.createElement("form", {
      className: "EOYSnippetForm",
      action: this.props.content.donation_form_url,
      method: this.props.form_method,
      onSubmit: this.handleSubmit,
      "data-metric": "EOYSnippetForm",
      ref: "form"
    }, donationURLParams.map(([key, value], idx) => /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "hidden",
      name: key,
      value: value,
      key: idx
    })), fieldNames.map((field, idx) => {
      const button_name = `donation_amount_${field}`;
      const amount = this.props.content[button_name];
      return /*#__PURE__*/external_React_default.a.createElement(external_React_default.a.Fragment, {
        key: idx
      }, /*#__PURE__*/external_React_default.a.createElement("input", {
        type: "radio",
        name: "amount",
        value: amount,
        id: field,
        defaultChecked: button_name === selected_button
      }), /*#__PURE__*/external_React_default.a.createElement("label", {
        htmlFor: field,
        className: "donation-amount"
      }, numberFormat.format(amount)));
    }), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "monthly-checkbox-container"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "monthly-checkbox",
      type: "checkbox"
    }), /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "monthly-checkbox"
    }, this.props.content.monthly_checkbox_label_text)), /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "hidden",
      name: "frequency",
      value: "single"
    }), /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "hidden",
      name: "currency",
      value: this.props.content.currency_code
    }), /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "hidden",
      name: "presets",
      value: fieldNames.map(field => this.props.content[`donation_amount_${field}`])
    }), /*#__PURE__*/external_React_default.a.createElement("button", {
      style: btnStyle,
      type: "submit",
      className: "ASRouterButton primary donation-form-url"
    }, this.props.content.button_label));
  }

  render() {
    const textStyle = {
      color: this.props.content.text_color,
      backgroundColor: this.props.content.background_color
    };
    const customElement = /*#__PURE__*/external_React_default.a.createElement("em", {
      style: {
        backgroundColor: this.props.content.highlight_color
      }
    });
    return /*#__PURE__*/external_React_default.a.createElement(SimpleSnippet_SimpleSnippet, EOYSnippet_extends({}, this.props, {
      className: this.props.content.test,
      customElements: {
        em: customElement
      },
      textStyle: textStyle,
      extraContent: this.renderDonations()
    }));
  }

}

const EOYSnippet = props => {
  const extendedContent = {
    monthly_checkbox_label_text: "Make my donation monthly",
    locale: "en-US",
    currency_code: "usd",
    selected_button: "donation_amount_second",
    ...props.content
  };
  return /*#__PURE__*/external_React_default.a.createElement(EOYSnippet_EOYSnippetBase, EOYSnippet_extends({}, props, {
    content: extendedContent,
    form_method: "GET"
  }));
};
// CONCATENATED MODULE: ./content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.jsx
function SubmitFormSnippet_extends() { SubmitFormSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SubmitFormSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






 // Alt text placeholder in case the prop from the server isn't available

const SubmitFormSnippet_ICON_ALT_TEXT = "";
class SubmitFormSnippet_SubmitFormSnippet extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.expandSnippet = this.expandSnippet.bind(this);
    this.handleSubmit = this.handleSubmit.bind(this);
    this.handleSubmitAttempt = this.handleSubmitAttempt.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.state = {
      expanded: false,
      submitAttempted: false,
      signupSubmitted: false,
      signupSuccess: false,
      disableForm: false
    };
  }

  handleSubmitAttempt() {
    if (!this.state.submitAttempted) {
      this.setState({
        submitAttempted: true
      });
    }
  }

  async handleSubmit(event) {
    let json;

    if (this.state.disableForm) {
      return;
    }

    event.preventDefault();
    this.setState({
      disableForm: true
    });
    this.props.sendUserActionTelemetry({
      event: "CLICK_BUTTON",
      event_context: "conversion-subscribe-activation",
      id: "NEWTAB_FOOTER_BAR_CONTENT"
    });

    if (this.props.form_method.toUpperCase() === "GET") {
      this.props.onBlock({
        preventDismiss: true
      });
      this.refs.form.submit();
      return;
    }

    const {
      url,
      formData
    } = this.props.processFormData ? this.props.processFormData(this.refs.mainInput, this.props) : {
      url: this.refs.form.action,
      formData: new FormData(this.refs.form)
    };

    try {
      const fetchRequest = new Request(url, {
        body: formData,
        method: "POST",
        credentials: "omit"
      });
      const response = await fetch(fetchRequest); // eslint-disable-line fetch-options/no-fetch-credentials

      json = await response.json();
    } catch (err) {
      console.log(err); // eslint-disable-line no-console
    }

    if (json && json.status === "ok") {
      this.setState({
        signupSuccess: true,
        signupSubmitted: true
      });

      if (!this.props.content.do_not_autoblock) {
        this.props.onBlock({
          preventDismiss: true
        });
      }

      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        event_context: "subscribe-success",
        id: "NEWTAB_FOOTER_BAR_CONTENT"
      });
    } else {
      // eslint-disable-next-line no-console
      console.error("There was a problem submitting the form", json || "[No JSON response]");
      this.setState({
        signupSuccess: false,
        signupSubmitted: true
      });
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        event_context: "subscribe-error",
        id: "NEWTAB_FOOTER_BAR_CONTENT"
      });
    }

    this.setState({
      disableForm: false
    });
  }

  expandSnippet() {
    this.props.sendUserActionTelemetry({
      event: "CLICK_BUTTON",
      event_context: "scene1-button-learn-more",
      id: this.props.UISurface
    });
    this.setState({
      expanded: true,
      signupSuccess: false,
      signupSubmitted: false
    });
  }

  renderHiddenFormInputs() {
    const {
      hidden_inputs
    } = this.props.content;

    if (!hidden_inputs) {
      return null;
    }

    return Object.keys(hidden_inputs).map((key, idx) => /*#__PURE__*/external_React_default.a.createElement("input", {
      key: idx,
      type: "hidden",
      name: key,
      value: hidden_inputs[key]
    }));
  }

  renderDisclaimer() {
    const {
      content
    } = this.props;

    if (!content.scene2_disclaimer_html) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement("p", {
      className: "disclaimerText"
    }, /*#__PURE__*/external_React_default.a.createElement(RichText, {
      text: content.scene2_disclaimer_html,
      localization_id: "disclaimer_html",
      links: content.links,
      doNotAutoBlock: true,
      openNewWindow: true,
      sendClick: this.props.sendClick
    }));
  }

  renderFormPrivacyNotice() {
    const {
      content
    } = this.props;

    if (!content.scene2_privacy_html) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement("p", {
      className: "privacyNotice"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "checkbox",
      id: "id_privacy",
      name: "privacy",
      required: "required"
    }), /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "id_privacy"
    }, /*#__PURE__*/external_React_default.a.createElement(RichText, {
      text: content.scene2_privacy_html,
      localization_id: "privacy_html",
      links: content.links,
      doNotAutoBlock: true,
      openNewWindow: true,
      sendClick: this.props.sendClick
    })));
  }

  renderSignupSubmitted() {
    const {
      content
    } = this.props;
    const isSuccess = this.state.signupSuccess;
    const successTitle = isSuccess && content.success_title;
    const bodyText = isSuccess ? {
      success_text: content.success_text
    } : {
      error_text: content.error_text
    };
    const retryButtonText = content.retry_button_label;
    return /*#__PURE__*/external_React_default.a.createElement(SnippetBase_SnippetBase, this.props, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "submissionStatus"
    }, successTitle ? /*#__PURE__*/external_React_default.a.createElement("h2", {
      className: "submitStatusTitle"
    }, successTitle) : null, /*#__PURE__*/external_React_default.a.createElement("p", null, /*#__PURE__*/external_React_default.a.createElement(RichText, SubmitFormSnippet_extends({}, bodyText, {
      localization_id: isSuccess ? "success_text" : "error_text"
    })), isSuccess ? null : /*#__PURE__*/external_React_default.a.createElement(Button, {
      onClick: this.expandSnippet
    }, retryButtonText))));
  }

  onInputChange(event) {
    if (!this.props.validateInput) {
      return;
    }

    const hasError = this.props.validateInput(event.target.value, this.props.content);
    event.target.setCustomValidity(hasError);
  }

  wrapSectionHeader(url) {
    return function (children) {
      return /*#__PURE__*/external_React_default.a.createElement("a", {
        href: url
      }, children);
    };
  }

  renderInput() {
    const placholder = this.props.content.scene2_email_placeholder_text || this.props.content.scene2_input_placeholder;
    return /*#__PURE__*/external_React_default.a.createElement("input", {
      ref: "mainInput",
      type: this.props.inputType || "email",
      className: `mainInput${this.state.submitAttempted ? "" : " clean"}`,
      name: "email",
      required: true,
      placeholder: placholder,
      onChange: this.props.validateInput ? this.onInputChange : null
    });
  }

  renderForm() {
    return /*#__PURE__*/external_React_default.a.createElement("form", {
      action: this.props.form_action,
      method: this.props.form_method,
      onSubmit: this.handleSubmit,
      ref: "form"
    }, this.renderHiddenFormInputs(), /*#__PURE__*/external_React_default.a.createElement("div", null, this.renderInput(), /*#__PURE__*/external_React_default.a.createElement("button", {
      type: "submit",
      className: "ASRouterButton primary",
      onClick: this.handleSubmitAttempt,
      ref: "formSubmitBtn"
    }, this.props.content.scene2_button_label)), this.renderFormPrivacyNotice() || this.renderDisclaimer());
  }

  renderScene2Icon() {
    const {
      content
    } = this.props;

    if (!content.scene2_icon) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "scene2Icon"
    }, /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(content.scene2_icon),
      className: "icon-light-theme",
      alt: content.scene2_icon_alt_text || SubmitFormSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(content.scene2_icon_dark_theme || content.scene2_icon),
      className: "icon-dark-theme",
      alt: content.scene2_icon_alt_text || SubmitFormSnippet_ICON_ALT_TEXT
    }));
  }

  renderSignupView() {
    const {
      content
    } = this.props;
    const containerClass = `SubmitFormSnippet ${this.props.className}`;
    return /*#__PURE__*/external_React_default.a.createElement(SnippetBase_SnippetBase, SubmitFormSnippet_extends({}, this.props, {
      className: containerClass,
      footerDismiss: true
    }), this.renderScene2Icon(), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "message"
    }, /*#__PURE__*/external_React_default.a.createElement("p", null, content.scene2_title && /*#__PURE__*/external_React_default.a.createElement("h3", {
      className: "scene2Title"
    }, content.scene2_title), " ", content.scene2_text && /*#__PURE__*/external_React_default.a.createElement(RichText, {
      scene2_text: content.scene2_text,
      localization_id: "scene2_text"
    }))), this.renderForm());
  }

  renderSectionHeader() {
    const {
      props
    } = this; // an icon and text must be specified to render the section header

    if (props.content.section_title_icon && props.content.section_title_text) {
      const sectionTitleIconLight = safeURI(props.content.section_title_icon);
      const sectionTitleIconDark = safeURI(props.content.section_title_icon_dark_theme || props.content.section_title_icon);
      const sectionTitleURL = props.content.section_title_url;
      return /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "section-header"
      }, /*#__PURE__*/external_React_default.a.createElement("h3", {
        className: "section-title"
      }, /*#__PURE__*/external_React_default.a.createElement(ConditionalWrapper_ConditionalWrapper, {
        wrap: this.wrapSectionHeader(sectionTitleURL),
        condition: sectionTitleURL
      }, /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "icon icon-small-spacer icon-light-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconLight}")`
        }
      }), /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "icon icon-small-spacer icon-dark-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconDark}")`
        }
      }), /*#__PURE__*/external_React_default.a.createElement("span", {
        className: "section-title-text"
      }, props.content.section_title_text))));
    }

    return null;
  }

  renderSignupViewAlt() {
    const {
      content
    } = this.props;
    const containerClass = `SubmitFormSnippet ${this.props.className} scene2Alt`;
    return /*#__PURE__*/external_React_default.a.createElement(SnippetBase_SnippetBase, SubmitFormSnippet_extends({}, this.props, {
      className: containerClass // Don't show bottom dismiss button
      ,
      footerDismiss: false
    }), this.renderSectionHeader(), this.renderScene2Icon(), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "message"
    }, /*#__PURE__*/external_React_default.a.createElement("p", null, content.scene2_text && /*#__PURE__*/external_React_default.a.createElement(RichText, {
      scene2_text: content.scene2_text,
      localization_id: "scene2_text"
    })), this.renderForm()));
  }

  getFirstSceneContent() {
    return Object.keys(this.props.content).filter(key => key.includes("scene1")).reduce((acc, key) => {
      acc[key.substr(7)] = this.props.content[key];
      return acc;
    }, {});
  }

  render() {
    const content = { ...this.props.content,
      ...this.getFirstSceneContent()
    };

    if (this.state.signupSubmitted) {
      return this.renderSignupSubmitted();
    } // Render only scene 2 (signup view). Must check before `renderSignupView`
    // to catch the Failure/Try again scenario where we want to return and render
    // the scene again.


    if (this.props.expandedAlt) {
      return this.renderSignupViewAlt();
    }

    if (this.state.expanded) {
      return this.renderSignupView();
    }

    return /*#__PURE__*/external_React_default.a.createElement(SimpleSnippet_SimpleSnippet, SubmitFormSnippet_extends({}, this.props, {
      content: content,
      onButtonClick: this.expandSnippet
    }));
  }

}
// CONCATENATED MODULE: ./content-src/asrouter/templates/FXASignupSnippet/FXASignupSnippet.jsx
function FXASignupSnippet_extends() { FXASignupSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return FXASignupSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const FXASignupSnippet = props => {
  const userAgent = window.navigator.userAgent.match(/Firefox\/([0-9]+)\./);
  const firefox_version = userAgent ? parseInt(userAgent[1], 10) : 0;
  const extendedContent = {
    scene1_button_label: "Learn more",
    retry_button_label: "Try again",
    scene2_email_placeholder_text: "Your email here",
    scene2_button_label: "Sign me up",
    scene2_dismiss_button_text: "Dismiss",
    ...props.content,
    hidden_inputs: {
      action: "email",
      context: "fx_desktop_v3",
      entrypoint: "snippets",
      utm_source: "snippet",
      utm_content: firefox_version,
      utm_campaign: props.content.utm_campaign,
      utm_term: props.content.utm_term,
      ...props.content.hidden_inputs
    }
  };
  return /*#__PURE__*/external_React_default.a.createElement(SubmitFormSnippet_SubmitFormSnippet, FXASignupSnippet_extends({}, props, {
    content: extendedContent,
    form_action: "https://accounts.firefox.com/",
    form_method: "GET"
  }));
};
// CONCATENATED MODULE: ./content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet.jsx
function NewsletterSnippet_extends() { NewsletterSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return NewsletterSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const NewsletterSnippet = props => {
  const extendedContent = {
    scene1_button_label: "Learn more",
    retry_button_label: "Try again",
    scene2_email_placeholder_text: "Your email here",
    scene2_button_label: "Sign me up",
    scene2_dismiss_button_text: "Dismiss",
    scene2_newsletter: "mozilla-foundation",
    ...props.content,
    hidden_inputs: {
      newsletters: props.content.scene2_newsletter || "mozilla-foundation",
      fmt: "H",
      lang: props.content.locale || "en-US",
      source_url: `https://snippets.mozilla.com/show/${props.id}`,
      ...props.content.hidden_inputs
    }
  };
  return /*#__PURE__*/external_React_default.a.createElement(SubmitFormSnippet_SubmitFormSnippet, NewsletterSnippet_extends({}, props, {
    content: extendedContent,
    form_action: "https://basket.mozilla.org/subscribe.json",
    form_method: "POST"
  }));
};
// CONCATENATED MODULE: ./content-src/asrouter/templates/SendToDeviceSnippet/isEmailOrPhoneNumber.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks if a given string is an email or phone number or neither
 * @param {string} val The user input
 * @param {ASRMessageContent} content .content property on ASR message
 * @returns {"email"|"phone"|""} The type of the input
 */
function isEmailOrPhoneNumber(val, content) {
  const {
    locale
  } = content; // http://emailregex.com/

  const email_re = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
  const check_email = email_re.test(val);
  let check_phone; // depends on locale

  switch (locale) {
    case "en-US":
    case "en-CA":
      // allow 10-11 digits in case user wants to enter country code
      check_phone = val.length >= 10 && val.length <= 11 && !isNaN(val);
      break;

    case "de":
      // allow between 2 and 12 digits for german phone numbers
      check_phone = val.length >= 2 && val.length <= 12 && !isNaN(val);
      break;
    // this case should never be hit, but good to have a fallback just in case

    default:
      check_phone = !isNaN(val);
      break;
  }

  if (check_email) {
    return "email";
  } else if (check_phone) {
    return "phone";
  }

  return "";
}
// CONCATENATED MODULE: ./content-src/asrouter/templates/SendToDeviceSnippet/SendToDeviceSnippet.jsx
function SendToDeviceSnippet_extends() { SendToDeviceSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SendToDeviceSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




function validateInput(value, content) {
  const type = isEmailOrPhoneNumber(value, content);
  return type ? "" : "Must be an email or a phone number.";
}

function processFormData(input, message) {
  const {
    content
  } = message;
  const type = content.include_sms ? isEmailOrPhoneNumber(input.value, content) : "email";
  const formData = new FormData();
  let url;

  if (type === "phone") {
    url = "https://basket.mozilla.org/news/subscribe_sms/";
    formData.append("mobile_number", input.value);
    formData.append("msg_name", content.message_id_sms);
    formData.append("country", content.country);
  } else if (type === "email") {
    url = "https://basket.mozilla.org/news/subscribe/";
    formData.append("email", input.value);
    formData.append("newsletters", content.message_id_email);
    formData.append("source_url", encodeURIComponent(`https://snippets.mozilla.com/show/${message.id}`));
  }

  formData.append("lang", content.locale);
  return {
    formData,
    url
  };
}

function addDefaultValues(props) {
  return { ...props,
    content: {
      scene1_button_label: "Learn more",
      retry_button_label: "Try again",
      scene2_dismiss_button_text: "Dismiss",
      scene2_button_label: "Send",
      scene2_input_placeholder: "Your email here",
      locale: "en-US",
      country: "us",
      message_id_email: "",
      include_sms: false,
      ...props.content
    }
  };
}

const SendToDeviceSnippet = props => {
  const propsWithDefaults = addDefaultValues(props);
  return /*#__PURE__*/external_React_default.a.createElement(SubmitFormSnippet_SubmitFormSnippet, SendToDeviceSnippet_extends({}, propsWithDefaults, {
    form_method: "POST",
    className: "send_to_device_snippet",
    inputType: propsWithDefaults.content.include_sms ? "text" : "email",
    validateInput: propsWithDefaults.content.include_sms ? validateInput : null,
    processFormData: processFormData
  }));
};
const SendToDeviceScene2Snippet = props => {
  return /*#__PURE__*/external_React_default.a.createElement(SendToDeviceSnippet, SendToDeviceSnippet_extends({
    expandedAlt: true
  }, props));
};
// CONCATENATED MODULE: ./content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.jsx
function SimpleBelowSearchSnippet_extends() { SimpleBelowSearchSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SimpleBelowSearchSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





const SimpleBelowSearchSnippet_DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png"; // Alt text placeholder in case the prop from the server isn't available

const SimpleBelowSearchSnippet_ICON_ALT_TEXT = "";
class SimpleBelowSearchSnippet_SimpleBelowSearchSnippet extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  renderText() {
    const {
      props
    } = this;
    return props.content.text ? /*#__PURE__*/external_React_default.a.createElement(RichText, {
      text: props.content.text,
      customElements: this.props.customElements,
      localization_id: "text",
      links: props.content.links,
      sendClick: props.sendClick
    }) : null;
  }

  renderTitle() {
    const {
      title
    } = this.props.content;
    return title ? /*#__PURE__*/external_React_default.a.createElement("h3", {
      className: "title title-inline"
    }, title, /*#__PURE__*/external_React_default.a.createElement("br", null)) : null;
  }

  async onButtonClick() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        id: this.props.UISurface
      });
    }

    const {
      button_url
    } = this.props.content; // If button_url is defined handle it as OPEN_URL action

    const type = this.props.content.button_action || button_url && "OPEN_URL";
    await this.props.onAction({
      type,
      data: {
        args: this.props.content.button_action_args || button_url
      }
    });

    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
  }

  _shouldRenderButton() {
    return this.props.content.button_action || this.props.onButtonClick || this.props.content.button_url;
  }

  renderButton() {
    const {
      props
    } = this;

    if (!this._shouldRenderButton()) {
      return null;
    }

    return /*#__PURE__*/external_React_default.a.createElement(Button, {
      onClick: props.onButtonClick || this.onButtonClick,
      color: props.content.button_color,
      backgroundColor: props.content.button_background_color
    }, props.content.button_label);
  }

  render() {
    const {
      props
    } = this;
    let className = "SimpleBelowSearchSnippet";
    let containerName = "below-search-snippet";

    if (props.className) {
      className += ` ${props.className}`;
    }

    if (this._shouldRenderButton()) {
      className += " withButton";
      containerName += " withButton";
    }

    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: containerName
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "snippet-hover-wrapper"
    }, /*#__PURE__*/external_React_default.a.createElement(SnippetBase_SnippetBase, SimpleBelowSearchSnippet_extends({}, props, {
      className: className,
      textStyle: this.props.textStyle
    }), /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(props.content.icon) || SimpleBelowSearchSnippet_DEFAULT_ICON_PATH,
      className: "icon icon-light-theme",
      alt: props.content.icon_alt_text || SimpleBelowSearchSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default.a.createElement("img", {
      src: safeURI(props.content.icon_dark_theme || props.content.icon) || SimpleBelowSearchSnippet_DEFAULT_ICON_PATH,
      className: "icon icon-dark-theme",
      alt: props.content.icon_alt_text || SimpleBelowSearchSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "textContainer"
    }, this.renderTitle(), /*#__PURE__*/external_React_default.a.createElement("p", {
      className: "body"
    }, this.renderText()), this.props.extraContent), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "buttonContainer"
    }, this.renderButton()))));
  }

}
// CONCATENATED MODULE: ./content-src/asrouter/templates/template-manifest.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "SnippetsTemplates", function() { return SnippetsTemplates; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





 // Key names matching schema name of templates

const SnippetsTemplates = {
  simple_snippet: SimpleSnippet_SimpleSnippet,
  newsletter_snippet: NewsletterSnippet,
  fxa_signup_snippet: FXASignupSnippet,
  send_to_device_snippet: SendToDeviceSnippet,
  send_to_device_scene2_snippet: SendToDeviceScene2Snippet,
  eoy_snippet: EOYSnippet,
  simple_below_search_snippet: SimpleBelowSearchSnippet_SimpleBelowSearchSnippet
};

/***/ }),
/* 71 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
var builtins_namespaceObject = {};
__webpack_require__.r(builtins_namespaceObject);
__webpack_require__.d(builtins_namespaceObject, "NUMBER", function() { return NUMBER; });
__webpack_require__.d(builtins_namespaceObject, "DATETIME", function() { return DATETIME; });

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
   * Formatted values are suitable for use outside of the `FluentBundle`.
   * This method can use `Intl` formatters memoized by the `FluentBundle`
   * instance passed as an argument.
   *
   * @param   {FluentBundle} [bundle]
   * @returns {string}
   */


  toString() {
    throw new Error("Subclasses of FluentType must implement toString.");
  }

}
class FluentNone extends FluentType {
  valueOf() {
    return null;
  }

  toString() {
    return `{${this.value || "???"}}`;
  }

}
class FluentNumber extends FluentType {
  constructor(value, opts) {
    super(parseFloat(value), opts);
  }

  toString(bundle) {
    try {
      const nf = bundle._memoizeIntlObject(Intl.NumberFormat, this.opts);

      return nf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
  }

}
class FluentDateTime extends FluentType {
  constructor(value, opts) {
    super(new Date(value), opts);
  }

  toString(bundle) {
    try {
      const dtf = bundle._memoizeIntlObject(Intl.DateTimeFormat, this.opts);

      return dtf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
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

function NUMBER([arg], opts) {
  if (arg instanceof FluentNone) {
    return arg;
  }

  if (arg instanceof FluentNumber) {
    return new FluentNumber(arg.valueOf(), merge(arg.opts, opts));
  }

  return new FluentNone("NUMBER()");
}
function DATETIME([arg], opts) {
  if (arg instanceof FluentNone) {
    return arg;
  }

  if (arg instanceof FluentDateTime) {
    return new FluentDateTime(arg.valueOf(), merge(arg.opts, opts));
  }

  return new FluentNone("DATETIME()");
}
// CONCATENATED MODULE: ./node_modules/fluent/src/resolver.js
/* global Intl */

/**
 * @overview
 *
 * The role of the Fluent resolver is to format a translation object to an
 * instance of `FluentType` or an array of instances.
 *
 * Translations can contain references to other messages or variables,
 * conditional logic in form of select expressions, traits which describe their
 * grammatical features, and can use Fluent builtins which make use of the
 * `Intl` formatters to format numbers, dates, lists and more into the
 * bundle's language. See the documentation of the Fluent syntax for more
 * information.
 *
 * In case of errors the resolver will try to salvage as much of the
 * translation as possible.  In rare situations where the resolver didn't know
 * how to recover from an error it will return an instance of `FluentNone`.
 *
 * All expressions resolve to an instance of `FluentType`. The caller should
 * use the `toString` method to convert the instance to a native value.
 *
 * All functions in this file pass around a special object called `scope`.
 * This object stores a set of elements used by all resolve functions:
 *
 *  * {FluentBundle} bundle
 *      bundle for which the given resolution is happening
 *  * {Object} args
 *      list of developer provided arguments that can be used
 *  * {Array} errors
 *      list of errors collected while resolving
 *  * {WeakSet} dirty
 *      Set of patterns already encountered during this resolution.
 *      This is used to prevent cyclic resolutions.
 */

 // Prevent expansion of too long placeables.

const MAX_PLACEABLE_LENGTH = 2500; // Unicode bidi isolation characters.

const FSI = "\u2068";
const PDI = "\u2069"; // Helper: match a variant key to the given selector.

function resolver_match(bundle, selector, key) {
  if (key === selector) {
    // Both are strings.
    return true;
  } // XXX Consider comparing options too, e.g. minimumFractionDigits.


  if (key instanceof FluentNumber && selector instanceof FluentNumber && key.value === selector.value) {
    return true;
  }

  if (selector instanceof FluentNumber && typeof key === "string") {
    let category = bundle._memoizeIntlObject(Intl.PluralRules, selector.opts).select(selector.value);

    if (key === category) {
      return true;
    }
  }

  return false;
} // Helper: resolve the default variant from a list of variants.


function getDefault(scope, variants, star) {
  if (variants[star]) {
    return Type(scope, variants[star]);
  }

  scope.errors.push(new RangeError("No default"));
  return new FluentNone();
} // Helper: resolve arguments to a call expression.


function getArguments(scope, args) {
  const positional = [];
  const named = {};

  for (const arg of args) {
    if (arg.type === "narg") {
      named[arg.name] = Type(scope, arg.value);
    } else {
      positional.push(Type(scope, arg));
    }
  }

  return [positional, named];
} // Resolve an expression to a Fluent type.


function Type(scope, expr) {
  // A fast-path for strings which are the most common case. Since they
  // natively have the `toString` method they can be used as if they were
  // a FluentType instance without incurring the cost of creating one.
  if (typeof expr === "string") {
    return scope.bundle._transform(expr);
  } // A fast-path for `FluentNone` which doesn't require any additional logic.


  if (expr instanceof FluentNone) {
    return expr;
  } // The Runtime AST (Entries) encodes patterns (complex strings with
  // placeables) as Arrays.


  if (Array.isArray(expr)) {
    return Pattern(scope, expr);
  }

  switch (expr.type) {
    case "str":
      return expr.value;

    case "num":
      return new FluentNumber(expr.value, {
        minimumFractionDigits: expr.precision
      });

    case "var":
      return VariableReference(scope, expr);

    case "mesg":
      return MessageReference(scope, expr);

    case "term":
      return TermReference(scope, expr);

    case "func":
      return FunctionReference(scope, expr);

    case "select":
      return SelectExpression(scope, expr);

    case undefined:
      {
        // If it's a node with a value, resolve the value.
        if (expr.value !== null && expr.value !== undefined) {
          return Type(scope, expr.value);
        }

        scope.errors.push(new RangeError("No value"));
        return new FluentNone();
      }

    default:
      return new FluentNone();
  }
} // Resolve a reference to a variable.


function VariableReference(scope, {
  name
}) {
  if (!scope.args || !scope.args.hasOwnProperty(name)) {
    if (scope.insideTermReference === false) {
      scope.errors.push(new ReferenceError(`Unknown variable: ${name}`));
    }

    return new FluentNone(`$${name}`);
  }

  const arg = scope.args[name]; // Return early if the argument already is an instance of FluentType.

  if (arg instanceof FluentType) {
    return arg;
  } // Convert the argument to a Fluent type.


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
      scope.errors.push(new TypeError(`Unsupported variable type: ${name}, ${typeof arg}`));
      return new FluentNone(`$${name}`);
  }
} // Resolve a reference to another message.


function MessageReference(scope, {
  name,
  attr
}) {
  const message = scope.bundle._messages.get(name);

  if (!message) {
    const err = new ReferenceError(`Unknown message: ${name}`);
    scope.errors.push(err);
    return new FluentNone(name);
  }

  if (attr) {
    const attribute = message.attrs && message.attrs[attr];

    if (attribute) {
      return Type(scope, attribute);
    }

    scope.errors.push(new ReferenceError(`Unknown attribute: ${attr}`));
    return new FluentNone(`${name}.${attr}`);
  }

  return Type(scope, message);
} // Resolve a call to a Term with key-value arguments.


function TermReference(scope, {
  name,
  attr,
  args
}) {
  const id = `-${name}`;

  const term = scope.bundle._terms.get(id);

  if (!term) {
    const err = new ReferenceError(`Unknown term: ${id}`);
    scope.errors.push(err);
    return new FluentNone(id);
  } // Every TermReference has its own args.


  const [, keyargs] = getArguments(scope, args);
  const local = { ...scope,
    args: keyargs,
    insideTermReference: true
  };

  if (attr) {
    const attribute = term.attrs && term.attrs[attr];

    if (attribute) {
      return Type(local, attribute);
    }

    scope.errors.push(new ReferenceError(`Unknown attribute: ${attr}`));
    return new FluentNone(`${id}.${attr}`);
  }

  return Type(local, term);
} // Resolve a call to a Function with positional and key-value arguments.


function FunctionReference(scope, {
  name,
  args
}) {
  // Some functions are built-in. Others may be provided by the runtime via
  // the `FluentBundle` constructor.
  const func = scope.bundle._functions[name] || builtins_namespaceObject[name];

  if (!func) {
    scope.errors.push(new ReferenceError(`Unknown function: ${name}()`));
    return new FluentNone(`${name}()`);
  }

  if (typeof func !== "function") {
    scope.errors.push(new TypeError(`Function ${name}() is not callable`));
    return new FluentNone(`${name}()`);
  }

  try {
    return func(...getArguments(scope, args));
  } catch (e) {
    // XXX Report errors.
    return new FluentNone(`${name}()`);
  }
} // Resolve a select expression to the member object.


function SelectExpression(scope, {
  selector,
  variants,
  star
}) {
  let sel = Type(scope, selector);

  if (sel instanceof FluentNone) {
    const variant = getDefault(scope, variants, star);
    return Type(scope, variant);
  } // Match the selector against keys of each variant, in order.


  for (const variant of variants) {
    const key = Type(scope, variant.key);

    if (resolver_match(scope.bundle, sel, key)) {
      return Type(scope, variant);
    }
  }

  const variant = getDefault(scope, variants, star);
  return Type(scope, variant);
} // Resolve a pattern (a complex string with placeables).


function Pattern(scope, ptn) {
  if (scope.dirty.has(ptn)) {
    scope.errors.push(new RangeError("Cyclic reference"));
    return new FluentNone();
  } // Tag the pattern as dirty for the purpose of the current resolution.


  scope.dirty.add(ptn);
  const result = []; // Wrap interpolations with Directional Isolate Formatting characters
  // only when the pattern has more than one element.

  const useIsolating = scope.bundle._useIsolating && ptn.length > 1;

  for (const elem of ptn) {
    if (typeof elem === "string") {
      result.push(scope.bundle._transform(elem));
      continue;
    }

    const part = Type(scope, elem).toString(scope.bundle);

    if (useIsolating) {
      result.push(FSI);
    }

    if (part.length > MAX_PLACEABLE_LENGTH) {
      scope.errors.push(new RangeError("Too many characters in placeable " + `(${part.length}, max allowed is ${MAX_PLACEABLE_LENGTH})`));
      result.push(part.slice(MAX_PLACEABLE_LENGTH));
    } else {
      result.push(part);
    }

    if (useIsolating) {
      result.push(PDI);
    }
  }

  scope.dirty.delete(ptn);
  return result.join("");
}
/**
 * Format a translation into a string.
 *
 * @param   {FluentBundle} bundle
 *    A FluentBundle instance which will be used to resolve the
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


function resolve(bundle, args, message, errors = []) {
  const scope = {
    bundle,
    args,
    errors,
    dirty: new WeakSet(),
    // TermReferences are resolved in a new scope.
    insideTermReference: false
  };
  return Type(scope, message).toString(bundle);
}
// CONCATENATED MODULE: ./node_modules/fluent/src/error.js
class FluentError extends Error {}
// CONCATENATED MODULE: ./node_modules/fluent/src/resource.js
 // This regex is used to iterate through the beginnings of messages and terms.
// With the /m flag, the ^ matches at the beginning of every line.

const RE_MESSAGE_START = /^(-?[a-zA-Z][\w-]*) *= */mg; // Both Attributes and Variants are parsed in while loops. These regexes are
// used to break out of them.

const RE_ATTRIBUTE_START = /\.([a-zA-Z][\w-]*) *= */y;
const RE_VARIANT_START = /\*?\[/y;
const RE_NUMBER_LITERAL = /(-?[0-9]+(?:\.([0-9]+))?)/y;
const RE_IDENTIFIER = /([a-zA-Z][\w-]*)/y;
const RE_REFERENCE = /([$-])?([a-zA-Z][\w-]*)(?:\.([a-zA-Z][\w-]*))?/y;
const RE_FUNCTION_NAME = /^[A-Z][A-Z0-9_-]*$/; // A "run" is a sequence of text or string literal characters which don't
// require any special handling. For TextElements such special characters are: {
// (starts a placeable), and line breaks which require additional logic to check
// if the next line is indented. For StringLiterals they are: \ (starts an
// escape sequence), " (ends the literal), and line breaks which are not allowed
// in StringLiterals. Note that string runs may be empty; text runs may not.

const RE_TEXT_RUN = /([^{}\n\r]+)/y;
const RE_STRING_RUN = /([^\\"\n\r]*)/y; // Escape sequences.

const RE_STRING_ESCAPE = /\\([\\"])/y;
const RE_UNICODE_ESCAPE = /\\u([a-fA-F0-9]{4})|\\U([a-fA-F0-9]{6})/y; // Used for trimming TextElements and indents.

const RE_LEADING_NEWLINES = /^\n+/;
const RE_TRAILING_SPACES = / +$/; // Used in makeIndent to strip spaces from blank lines and normalize CRLF to LF.

const RE_BLANK_LINES = / *\r?\n/g; // Used in makeIndent to measure the indentation.

const RE_INDENT = /( *)$/; // Common tokens.

const TOKEN_BRACE_OPEN = /{\s*/y;
const TOKEN_BRACE_CLOSE = /\s*}/y;
const TOKEN_BRACKET_OPEN = /\[\s*/y;
const TOKEN_BRACKET_CLOSE = /\s*] */y;
const TOKEN_PAREN_OPEN = /\s*\(\s*/y;
const TOKEN_ARROW = /\s*->\s*/y;
const TOKEN_COLON = /\s*:\s*/y; // Note the optional comma. As a deviation from the Fluent EBNF, the parser
// doesn't enforce commas between call arguments.

const TOKEN_COMMA = /\s*,?\s*/y;
const TOKEN_BLANK = /\s+/y; // Maximum number of placeables in a single Pattern to protect against Quadratic
// Blowup attacks. See https://msdn.microsoft.com/en-us/magazine/ee335713.aspx.

const MAX_PLACEABLES = 100;
/**
 * Fluent Resource is a structure storing a map of parsed localization entries.
 */

class resource_FluentResource extends Map {
  /**
   * Create a new FluentResource from Fluent code.
   */
  static fromString(source) {
    RE_MESSAGE_START.lastIndex = 0;
    let resource = new this();
    let cursor = 0; // Iterate over the beginnings of messages and terms to efficiently skip
    // comments and recover from errors.

    while (true) {
      let next = RE_MESSAGE_START.exec(source);

      if (next === null) {
        break;
      }

      cursor = RE_MESSAGE_START.lastIndex;

      try {
        resource.set(next[1], parseMessage());
      } catch (err) {
        if (err instanceof FluentError) {
          // Don't report any Fluent syntax errors. Skip directly to the
          // beginning of the next message or term.
          continue;
        }

        throw err;
      }
    }

    return resource; // The parser implementation is inlined below for performance reasons.
    // The parser focuses on minimizing the number of false negatives at the
    // expense of increasing the risk of false positives. In other words, it
    // aims at parsing valid Fluent messages with a success rate of 100%, but it
    // may also parse a few invalid messages which the reference parser would
    // reject. The parser doesn't perform any validation and may produce entries
    // which wouldn't make sense in the real world. For best results users are
    // advised to validate translations with the fluent-syntax parser
    // pre-runtime.
    // The parser makes an extensive use of sticky regexes which can be anchored
    // to any offset of the source string without slicing it. Errors are thrown
    // to bail out of parsing of ill-formed messages.

    function test(re) {
      re.lastIndex = cursor;
      return re.test(source);
    } // Advance the cursor by the char if it matches. May be used as a predicate
    // (was the match found?) or, if errorClass is passed, as an assertion.


    function consumeChar(char, errorClass) {
      if (source[cursor] === char) {
        cursor++;
        return true;
      }

      if (errorClass) {
        throw new errorClass(`Expected ${char}`);
      }

      return false;
    } // Advance the cursor by the token if it matches. May be used as a predicate
    // (was the match found?) or, if errorClass is passed, as an assertion.


    function consumeToken(re, errorClass) {
      if (test(re)) {
        cursor = re.lastIndex;
        return true;
      }

      if (errorClass) {
        throw new errorClass(`Expected ${re.toString()}`);
      }

      return false;
    } // Execute a regex, advance the cursor, and return all capture groups.


    function match(re) {
      re.lastIndex = cursor;
      let result = re.exec(source);

      if (result === null) {
        throw new FluentError(`Expected ${re.toString()}`);
      }

      cursor = re.lastIndex;
      return result;
    } // Execute a regex, advance the cursor, and return the capture group.


    function match1(re) {
      return match(re)[1];
    }

    function parseMessage() {
      let value = parsePattern();
      let attrs = parseAttributes();

      if (attrs === null) {
        if (value === null) {
          throw new FluentError("Expected message value or attributes");
        }

        return value;
      }

      return {
        value,
        attrs
      };
    }

    function parseAttributes() {
      let attrs = {};

      while (test(RE_ATTRIBUTE_START)) {
        let name = match1(RE_ATTRIBUTE_START);
        let value = parsePattern();

        if (value === null) {
          throw new FluentError("Expected attribute value");
        }

        attrs[name] = value;
      }

      return Object.keys(attrs).length > 0 ? attrs : null;
    }

    function parsePattern() {
      // First try to parse any simple text on the same line as the id.
      if (test(RE_TEXT_RUN)) {
        var first = match1(RE_TEXT_RUN);
      } // If there's a placeable on the first line, parse a complex pattern.


      if (source[cursor] === "{" || source[cursor] === "}") {
        // Re-use the text parsed above, if possible.
        return parsePatternElements(first ? [first] : [], Infinity);
      } // RE_TEXT_VALUE stops at newlines. Only continue parsing the pattern if
      // what comes after the newline is indented.


      let indent = parseIndent();

      if (indent) {
        if (first) {
          // If there's text on the first line, the blank block is part of the
          // translation content in its entirety.
          return parsePatternElements([first, indent], indent.length);
        } // Otherwise, we're dealing with a block pattern, i.e. a pattern which
        // starts on a new line. Discrad the leading newlines but keep the
        // inline indent; it will be used by the dedentation logic.


        indent.value = trim(indent.value, RE_LEADING_NEWLINES);
        return parsePatternElements([indent], indent.length);
      }

      if (first) {
        // It was just a simple inline text after all.
        return trim(first, RE_TRAILING_SPACES);
      }

      return null;
    } // Parse a complex pattern as an array of elements.


    function parsePatternElements(elements = [], commonIndent) {
      let placeableCount = 0;

      while (true) {
        if (test(RE_TEXT_RUN)) {
          elements.push(match1(RE_TEXT_RUN));
          continue;
        }

        if (source[cursor] === "{") {
          if (++placeableCount > MAX_PLACEABLES) {
            throw new FluentError("Too many placeables");
          }

          elements.push(parsePlaceable());
          continue;
        }

        if (source[cursor] === "}") {
          throw new FluentError("Unbalanced closing brace");
        }

        let indent = parseIndent();

        if (indent) {
          elements.push(indent);
          commonIndent = Math.min(commonIndent, indent.length);
          continue;
        }

        break;
      }

      let lastIndex = elements.length - 1; // Trim the trailing spaces in the last element if it's a TextElement.

      if (typeof elements[lastIndex] === "string") {
        elements[lastIndex] = trim(elements[lastIndex], RE_TRAILING_SPACES);
      }

      let baked = [];

      for (let element of elements) {
        if (element.type === "indent") {
          // Dedent indented lines by the maximum common indent.
          element = element.value.slice(0, element.value.length - commonIndent);
        } else if (element.type === "str") {
          // Optimize StringLiterals into their value.
          element = element.value;
        }

        if (element) {
          baked.push(element);
        }
      }

      return baked;
    }

    function parsePlaceable() {
      consumeToken(TOKEN_BRACE_OPEN, FluentError);
      let selector = parseInlineExpression();

      if (consumeToken(TOKEN_BRACE_CLOSE)) {
        return selector;
      }

      if (consumeToken(TOKEN_ARROW)) {
        let variants = parseVariants();
        consumeToken(TOKEN_BRACE_CLOSE, FluentError);
        return {
          type: "select",
          selector,
          ...variants
        };
      }

      throw new FluentError("Unclosed placeable");
    }

    function parseInlineExpression() {
      if (source[cursor] === "{") {
        // It's a nested placeable.
        return parsePlaceable();
      }

      if (test(RE_REFERENCE)) {
        let [, sigil, name, attr = null] = match(RE_REFERENCE);

        if (sigil === "$") {
          return {
            type: "var",
            name
          };
        }

        if (consumeToken(TOKEN_PAREN_OPEN)) {
          let args = parseArguments();

          if (sigil === "-") {
            // A parameterized term: -term(...).
            return {
              type: "term",
              name,
              attr,
              args
            };
          }

          if (RE_FUNCTION_NAME.test(name)) {
            return {
              type: "func",
              name,
              args
            };
          }

          throw new FluentError("Function names must be all upper-case");
        }

        if (sigil === "-") {
          // A non-parameterized term: -term.
          return {
            type: "term",
            name,
            attr,
            args: []
          };
        }

        return {
          type: "mesg",
          name,
          attr
        };
      }

      return parseLiteral();
    }

    function parseArguments() {
      let args = [];

      while (true) {
        switch (source[cursor]) {
          case ")":
            // End of the argument list.
            cursor++;
            return args;

          case undefined:
            // EOF
            throw new FluentError("Unclosed argument list");
        }

        args.push(parseArgument()); // Commas between arguments are treated as whitespace.

        consumeToken(TOKEN_COMMA);
      }
    }

    function parseArgument() {
      let expr = parseInlineExpression();

      if (expr.type !== "mesg") {
        return expr;
      }

      if (consumeToken(TOKEN_COLON)) {
        // The reference is the beginning of a named argument.
        return {
          type: "narg",
          name: expr.name,
          value: parseLiteral()
        };
      } // It's a regular message reference.


      return expr;
    }

    function parseVariants() {
      let variants = [];
      let count = 0;
      let star;

      while (test(RE_VARIANT_START)) {
        if (consumeChar("*")) {
          star = count;
        }

        let key = parseVariantKey();
        let value = parsePattern();

        if (value === null) {
          throw new FluentError("Expected variant value");
        }

        variants[count++] = {
          key,
          value
        };
      }

      if (count === 0) {
        return null;
      }

      if (star === undefined) {
        throw new FluentError("Expected default variant");
      }

      return {
        variants,
        star
      };
    }

    function parseVariantKey() {
      consumeToken(TOKEN_BRACKET_OPEN, FluentError);
      let key = test(RE_NUMBER_LITERAL) ? parseNumberLiteral() : match1(RE_IDENTIFIER);
      consumeToken(TOKEN_BRACKET_CLOSE, FluentError);
      return key;
    }

    function parseLiteral() {
      if (test(RE_NUMBER_LITERAL)) {
        return parseNumberLiteral();
      }

      if (source[cursor] === "\"") {
        return parseStringLiteral();
      }

      throw new FluentError("Invalid expression");
    }

    function parseNumberLiteral() {
      let [, value, fraction = ""] = match(RE_NUMBER_LITERAL);
      let precision = fraction.length;
      return {
        type: "num",
        value: parseFloat(value),
        precision
      };
    }

    function parseStringLiteral() {
      consumeChar("\"", FluentError);
      let value = "";

      while (true) {
        value += match1(RE_STRING_RUN);

        if (source[cursor] === "\\") {
          value += parseEscapeSequence();
          continue;
        }

        if (consumeChar("\"")) {
          return {
            type: "str",
            value
          };
        } // We've reached an EOL of EOF.


        throw new FluentError("Unclosed string literal");
      }
    } // Unescape known escape sequences.


    function parseEscapeSequence() {
      if (test(RE_STRING_ESCAPE)) {
        return match1(RE_STRING_ESCAPE);
      }

      if (test(RE_UNICODE_ESCAPE)) {
        let [, codepoint4, codepoint6] = match(RE_UNICODE_ESCAPE);
        let codepoint = parseInt(codepoint4 || codepoint6, 16);
        return codepoint <= 0xD7FF || 0xE000 <= codepoint // It's a Unicode scalar value.
        ? String.fromCodePoint(codepoint) // Lonely surrogates can cause trouble when the parsing result is
        // saved using UTF-8. Use U+FFFD REPLACEMENT CHARACTER instead.
        : "�";
      }

      throw new FluentError("Unknown escape sequence");
    } // Parse blank space. Return it if it looks like indent before a pattern
    // line. Skip it othwerwise.


    function parseIndent() {
      let start = cursor;
      consumeToken(TOKEN_BLANK); // Check the first non-blank character after the indent.

      switch (source[cursor]) {
        case ".":
        case "[":
        case "*":
        case "}":
        case undefined:
          // EOF
          // A special character. End the Pattern.
          return false;

        case "{":
          // Placeables don't require indentation (in EBNF: block-placeable).
          // Continue the Pattern.
          return makeIndent(source.slice(start, cursor));
      } // If the first character on the line is not one of the special characters
      // listed above, it's a regular text character. Check if there's at least
      // one space of indent before it.


      if (source[cursor - 1] === " ") {
        // It's an indented text character (in EBNF: indented-char). Continue
        // the Pattern.
        return makeIndent(source.slice(start, cursor));
      } // A not-indented text character is likely the identifier of the next
      // message. End the Pattern.


      return false;
    } // Trim blanks in text according to the given regex.


    function trim(text, re) {
      return text.replace(re, "");
    } // Normalize a blank block and extract the indent details.


    function makeIndent(blank) {
      let value = blank.replace(RE_BLANK_LINES, "\n");
      let length = RE_INDENT.exec(blank)[1].length;
      return {
        type: "indent",
        value,
        length
      };
    }
  }

}
// CONCATENATED MODULE: ./node_modules/fluent/src/bundle.js


/**
 * Message bundles are single-language stores of translations.  They are
 * responsible for parsing translation resources in the Fluent syntax and can
 * format translation units (entities) to strings.
 *
 * Always use `FluentBundle.format` to retrieve translation units from a
 * bundle. Translations can contain references to other entities or variables,
 * conditional logic in form of select expressions, traits which describe their
 * grammatical features, and can use Fluent builtins which make use of the
 * `Intl` formatters to format numbers, dates, lists and more into the
 * bundle's language. See the documentation of the Fluent syntax for more
 * information.
 */

class bundle_FluentBundle {
  /**
   * Create an instance of `FluentBundle`.
   *
   * The `locales` argument is used to instantiate `Intl` formatters used by
   * translations.  The `options` object can be used to configure the bundle.
   *
   * Examples:
   *
   *     const bundle = new FluentBundle(locales);
   *
   *     const bundle = new FluentBundle(locales, { useIsolating: false });
   *
   *     const bundle = new FluentBundle(locales, {
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
   *                    Default: true
   *
   *   - `transform` - a function used to transform string parts of patterns.
   *
   * @param   {string|Array<string>} locales - Locale or locales of the bundle
   * @param   {Object} [options]
   * @returns {FluentBundle}
   */
  constructor(locales, {
    functions = {},
    useIsolating = true,
    transform = v => v
  } = {}) {
    this.locales = Array.isArray(locales) ? locales : [locales];
    this._terms = new Map();
    this._messages = new Map();
    this._functions = functions;
    this._useIsolating = useIsolating;
    this._transform = transform;
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
   * Check if a message is present in the bundle.
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
   * `FluentBundle.format`.
   *
   * @param {string} id - The identifier of the message to check.
   * @returns {Any}
   */


  getMessage(id) {
    return this._messages.get(id);
  }
  /**
   * Add a translation resource to the bundle.
   *
   * The translation resource must use the Fluent syntax.  It will be parsed by
   * the bundle and each translation unit (message) will be available in the
   * bundle by its identifier.
   *
   *     bundle.addMessages('foo = Foo');
   *     bundle.getMessage('foo');
   *
   *     // Returns a raw representation of the 'foo' message.
   *
   *     bundle.addMessages('bar = Bar');
   *     bundle.addMessages('bar = Newbar', { allowOverrides: true });
   *     bundle.getMessage('bar');
   *
   *     // Returns a raw representation of the 'bar' message: Newbar.
   *
   * Parsed entities should be formatted with the `format` method in case they
   * contain logic (references, select expressions etc.).
   *
   * Available options:
   *
   *   - `allowOverrides` - boolean specifying whether it's allowed to override
   *                      an existing message or term with a new value.
   *                      Default: false
   *
   * @param   {string} source - Text resource with translations.
   * @param   {Object} [options]
   * @returns {Array<Error>}
   */


  addMessages(source, options) {
    const res = resource_FluentResource.fromString(source);
    return this.addResource(res, options);
  }
  /**
   * Add a translation resource to the bundle.
   *
   * The translation resource must be an instance of FluentResource,
   * e.g. parsed by `FluentResource.fromString`.
   *
   *     let res = FluentResource.fromString("foo = Foo");
   *     bundle.addResource(res);
   *     bundle.getMessage('foo');
   *
   *     // Returns a raw representation of the 'foo' message.
   *
   *     let res = FluentResource.fromString("bar = Bar");
   *     bundle.addResource(res);
   *     res = FluentResource.fromString("bar = Newbar");
   *     bundle.addResource(res, { allowOverrides: true });
   *     bundle.getMessage('bar');
   *
   *     // Returns a raw representation of the 'bar' message: Newbar.
   *
   * Parsed entities should be formatted with the `format` method in case they
   * contain logic (references, select expressions etc.).
   *
   * Available options:
   *
   *   - `allowOverrides` - boolean specifying whether it's allowed to override
   *                      an existing message or term with a new value.
   *                      Default: false
   *
   * @param   {FluentResource} res - FluentResource object.
   * @param   {Object} [options]
   * @returns {Array<Error>}
   */


  addResource(res, {
    allowOverrides = false
  } = {}) {
    const errors = [];

    for (const [id, value] of res) {
      if (id.startsWith("-")) {
        // Identifiers starting with a dash (-) define terms. Terms are private
        // and cannot be retrieved from FluentBundle.
        if (allowOverrides === false && this._terms.has(id)) {
          errors.push(`Attempt to override an existing term: "${id}"`);
          continue;
        }

        this._terms.set(id, value);
      } else {
        if (allowOverrides === false && this._messages.has(id)) {
          errors.push(`Attempt to override an existing message: "${id}"`);
          continue;
        }

        this._messages.set(id, value);
      }
    }

    return errors;
  }
  /**
   * Format a message to a string or null.
   *
   * Format a raw `message` from the bundle into a string (or a null if it has
   * a null value).  `args` will be used to resolve references to variables
   * passed as arguments to the translation.
   *
   * In case of errors `format` will try to salvage as much of the translation
   * as possible and will still return a string.  For performance reasons, the
   * encountered errors are not returned but instead are appended to the
   * `errors` array passed as the third argument.
   *
   *     const errors = [];
   *     bundle.addMessages('hello = Hello, { $name }!');
   *     const hello = bundle.getMessage('hello');
   *     bundle.format(hello, { name: 'Jane' }, errors);
   *
   *     // Returns 'Hello, Jane!' and `errors` is empty.
   *
   *     bundle.format(hello, undefined, errors);
   *
   *     // Returns 'Hello, name!' and `errors` is now:
   *
   *     [<ReferenceError: Unknown variable: name>]
   *
   * @param   {Object | string}    message
   * @param   {Object | undefined} args
   * @param   {Array}              errors
   * @returns {?string}
   */


  format(message, args, errors) {
    // optimize entities which are simple strings with no attributes
    if (typeof message === "string") {
      return this._transform(message);
    } // optimize entities with null values


    if (message === null || message.value === null) {
      return null;
    } // optimize simple-string entities with attributes


    if (typeof message.value === "string") {
      return this._transform(message.value);
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
// CONCATENATED MODULE: ./node_modules/fluent/src/index.js
/*
 * @module fluent
 * @overview
 *
 * `fluent` is a JavaScript implementation of Project Fluent, a localization
 * framework designed to unleash the expressive power of the natural language.
 *
 */




// CONCATENATED MODULE: ./content-src/asrouter/rich-text-strings.js
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "RICH_TEXT_KEYS", function() { return RICH_TEXT_KEYS; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "generateBundles", function() { return generateBundles; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Properties that allow rich text MUST be added to this list.
 *   key: the localization_id that should be used
 *   value: a property or array of properties on the message.content object
 */

const RICH_TEXT_CONFIG = {
  text: ["text", "scene1_text"],
  success_text: "success_text",
  error_text: "error_text",
  scene2_text: "scene2_text",
  amo_html: "amo_html",
  privacy_html: "scene2_privacy_html",
  disclaimer_html: "scene2_disclaimer_html"
};
const RICH_TEXT_KEYS = Object.keys(RICH_TEXT_CONFIG);
/**
 * Generates an array of messages suitable for fluent's localization provider
 * including all needed strings for rich text.
 * @param {object} content A .content object from an ASR message (i.e. message.content)
 * @returns {FluentBundle[]} A array containing the fluent message context
 */

function generateBundles(content) {
  const bundle = new bundle_FluentBundle("en-US");
  RICH_TEXT_KEYS.forEach(key => {
    const attrs = RICH_TEXT_CONFIG[key];
    const attrsToTry = Array.isArray(attrs) ? [...attrs] : [attrs];
    let string = "";

    while (!string && attrsToTry.length) {
      const attr = attrsToTry.pop();
      string = content[attr];
    }

    bundle.addMessages(`${key} = ${string}`);
  });
  return [bundle];
}

/***/ }),
/* 72 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(8);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/ThemesSection/ThemesSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class ThemesSection_ThemesSection extends external_React_default.a.PureComponent {
  render() {
    return /*#__PURE__*/external_React_default.a.createElement("div", null);
  }

}
// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/BackgroundsSection/BackgroundsSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class BackgroundsSection_BackgroundsSection extends external_React_default.a.PureComponent {
  render() {
    return /*#__PURE__*/external_React_default.a.createElement("div", null);
  }

}
// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(1);

// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/ContentSection/ContentSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class ContentSection_ContentSection extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.onPreferenceSelect = this.onPreferenceSelect.bind(this);
  }

  inputUserEvent(eventSource, status) {
    this.props.dispatch(Actions["actionCreators"].UserEvent({
      event: "PREF_CHANGED",
      source: eventSource,
      value: {
        status,
        menu_source: "CUSTOMIZE_MENU"
      }
    }));
  }

  onPreferenceSelect(e) {
    let prefName = e.target.getAttribute("preference");
    const eventSource = e.target.getAttribute("eventSource");
    let value;

    if (e.target.nodeName === "SELECT") {
      value = parseInt(e.target.value, 10);
    } else if (e.target.nodeName === "INPUT") {
      value = e.target.checked;

      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    }

    this.props.setPref(prefName, value);
  }

  render() {
    const {
      topSitesEnabled,
      pocketEnabled,
      highlightsEnabled,
      snippetsEnabled,
      showSponsoredTopSitesEnabled,
      showSponsoredPocketEnabled,
      topSitesRowsCount
    } = this.props.enabledSections;
    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "home-section"
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      id: "shortcuts-section",
      className: "section"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "shortcuts-toggle",
      checked: topSitesEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "feeds.topsites",
      "aria-labelledby": "custom-shortcuts-title",
      "aria-describedby": "custom-shortcuts-subtitle",
      eventSource: "TOP_SITES"
    }), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "slider",
      role: "presentation"
    })), /*#__PURE__*/external_React_default.a.createElement("div", null, /*#__PURE__*/external_React_default.a.createElement("h2", {
      id: "custom-shortcuts-title",
      className: "title"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "shortcuts-toggle",
      "data-l10n-id": "newtab-custom-shortcuts-title"
    })), /*#__PURE__*/external_React_default.a.createElement("p", {
      id: "custom-shortcuts-subtitle",
      className: "subtitle",
      "data-l10n-id": "newtab-custom-shortcuts-subtitle"
    }), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: `more-info-top-wrapper ${topSitesEnabled ? "" : "shrink"}`
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: `more-information ${topSitesEnabled ? "expand" : "shrink"}`
    }, /*#__PURE__*/external_React_default.a.createElement("select", {
      id: "row-selector",
      className: "selector",
      name: "row-count",
      preference: "topSitesRows",
      value: topSitesRowsCount,
      onChange: this.onPreferenceSelect,
      disabled: !topSitesEnabled,
      "aria-labelledby": "custom-shortcuts-title"
    }, /*#__PURE__*/external_React_default.a.createElement("option", {
      value: "1",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 1}"
    }), /*#__PURE__*/external_React_default.a.createElement("option", {
      value: "2",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 2}"
    }), /*#__PURE__*/external_React_default.a.createElement("option", {
      value: "3",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 3}"
    }), /*#__PURE__*/external_React_default.a.createElement("option", {
      value: "4",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 4}"
    })), this.props.mayHaveSponsoredTopSites && /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "check-wrapper",
      role: "presentation"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "sponsored-shortcuts",
      className: "sponsored-checkbox",
      disabled: !topSitesEnabled,
      checked: showSponsoredTopSitesEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "showSponsoredTopSites",
      eventSource: "SPONSORED_TOP_SITES"
    }), /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "sponsored",
      htmlFor: "sponsored-shortcuts",
      "data-l10n-id": "newtab-custom-sponsored-sites"
    })))))), this.props.pocketRegion && /*#__PURE__*/external_React_default.a.createElement("div", {
      id: "pocket-section",
      className: "section"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "pocket-toggle",
      checked: pocketEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "feeds.section.topstories",
      "aria-labelledby": "custom-pocket-title",
      "aria-describedby": "custom-pocket-subtitle",
      eventSource: "TOP_STORIES"
    }), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "slider",
      role: "presentation"
    })), /*#__PURE__*/external_React_default.a.createElement("div", null, /*#__PURE__*/external_React_default.a.createElement("h2", {
      id: "custom-pocket-title",
      className: "title"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "pocket-toggle",
      "data-l10n-id": "newtab-custom-pocket-title"
    })), /*#__PURE__*/external_React_default.a.createElement("p", {
      id: "custom-pocket-subtitle",
      className: "subtitle",
      "data-l10n-id": "newtab-custom-pocket-subtitle"
    }), this.props.mayHaveSponsoredStories && /*#__PURE__*/external_React_default.a.createElement("div", {
      className: `more-info-pocket-wrapper ${pocketEnabled ? "" : "shrink"}`
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: `more-information ${pocketEnabled ? "expand" : "shrink"}`
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "check-wrapper",
      role: "presentation"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "sponsored-pocket",
      className: "sponsored-checkbox",
      disabled: !pocketEnabled,
      checked: showSponsoredPocketEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "showSponsored",
      eventSource: "POCKET_SPOCS"
    }), /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "sponsored",
      htmlFor: "sponsored-pocket",
      "data-l10n-id": "newtab-custom-pocket-sponsored"
    })))))), /*#__PURE__*/external_React_default.a.createElement("div", {
      id: "recent-section",
      className: "section"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "highlights-toggle",
      checked: highlightsEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "feeds.section.highlights",
      eventSource: "HIGHLIGHTS",
      "aria-labelledby": "custom-recent-title",
      "aria-describedby": "custom-recent-subtitle"
    }), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "slider",
      role: "presentation"
    })), /*#__PURE__*/external_React_default.a.createElement("div", null, /*#__PURE__*/external_React_default.a.createElement("h2", {
      id: "custom-recent-title",
      className: "title"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "highlights-toggle",
      "data-l10n-id": "newtab-custom-recent-title"
    })), /*#__PURE__*/external_React_default.a.createElement("p", {
      id: "custom-recent-subtitle",
      className: "subtitle",
      "data-l10n-id": "newtab-custom-recent-subtitle"
    }))), /*#__PURE__*/external_React_default.a.createElement("div", {
      id: "snippets-section",
      className: "section"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      id: "snippets-toggle",
      checked: snippetsEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "feeds.snippets",
      "aria-labelledby": "custom-snippets-title",
      "aria-describedby": "custom-snippets-subtitle",
      eventSource: "SNIPPETS"
    }), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "slider",
      role: "presentation"
    })), /*#__PURE__*/external_React_default.a.createElement("div", null, /*#__PURE__*/external_React_default.a.createElement("h2", {
      id: "custom-snippets-title",
      className: "title"
    }, /*#__PURE__*/external_React_default.a.createElement("label", {
      htmlFor: "snippets-toggle",
      "data-l10n-id": "newtab-custom-snippets-title"
    })), /*#__PURE__*/external_React_default.a.createElement("p", {
      id: "custom-snippets-subtitle",
      className: "subtitle",
      "data-l10n-id": "newtab-custom-snippets-subtitle"
    }))), /*#__PURE__*/external_React_default.a.createElement("span", {
      className: "divider",
      role: "separator"
    }), /*#__PURE__*/external_React_default.a.createElement("div", null, /*#__PURE__*/external_React_default.a.createElement("button", {
      id: "settings-link",
      className: "external-link",
      onClick: this.props.openPreferences,
      "data-l10n-id": "newtab-custom-settings"
    })));
  }

}
// EXTERNAL MODULE: external "ReactRedux"
var external_ReactRedux_ = __webpack_require__(7);

// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/CustomizeMenu.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "_CustomizeMenu", function() { return CustomizeMenu_CustomizeMenu; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "CustomizeMenu", function() { return CustomizeMenu; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





class CustomizeMenu_CustomizeMenu extends external_React_default.a.PureComponent {
  render() {
    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "customize-menu"
    }, /*#__PURE__*/external_React_default.a.createElement("button", {
      onClick: this.props.onClose,
      className: "close-button",
      "data-l10n-id": "newtab-custom-close-button"
    }), /*#__PURE__*/external_React_default.a.createElement(ThemesSection_ThemesSection, null), /*#__PURE__*/external_React_default.a.createElement(BackgroundsSection_BackgroundsSection, null), /*#__PURE__*/external_React_default.a.createElement(ContentSection_ContentSection, {
      openPreferences: this.props.openPreferences,
      setPref: this.props.setPref,
      enabledSections: this.props.enabledSections,
      pocketRegion: this.props.pocketRegion,
      mayHaveSponsoredTopSites: this.props.mayHaveSponsoredTopSites,
      mayHaveSponsoredStories: this.props.DiscoveryStream.config.show_spocs,
      dispatch: this.props.dispatch
    }));
  }

}
const CustomizeMenu = Object(external_ReactRedux_["connect"])(state => ({
  DiscoveryStream: state.DiscoveryStream
}))(CustomizeMenu_CustomizeMenu);

/***/ }),
/* 73 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(1);

// CONCATENATED MODULE: ./common/Dedupe.jsm
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
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
const PREF_PERSONALIZATION_VERSION = "discoverystream.personalization.version";
const PREF_COLLECTION_DISMISSIBLE = "discoverystream.isCollectionDismissible";
const dedupe = new Dedupe(site => site && site.url);
const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    locale: ""
  },
  ASRouter: {
    initialized: false
  },
  Snippets: {
    initialized: false
  },
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: [],
    // Used in content only to dispatch action to TopSiteForm.
    editForm: null,
    // Used in content only to open the SearchShortcutsForm modal.
    showSearchShortcutsForm: false,
    // The list of available search shortcuts.
    searchShortcuts: []
  },
  Prefs: {
    initialized: false,
    values: {
      featureConfig: {}
    }
  },
  Dialog: {
    visible: false,
    data: {}
  },
  Sections: [],
  Pocket: {
    isUserLoggedIn: null,
    pocketCta: {},
    waitingForSpoc: true
  },
  // This is the new pocket configurable layout state.
  DiscoveryStream: {
    // This is a JSON-parsed copy of the discoverystream.config pref value.
    config: {
      enabled: false,
      layout_endpoint: ""
    },
    layout: [],
    lastUpdated: null,
    isPrivacyInfoModalVisible: false,
    isCollectionDismissible: false,
    feeds: {
      data: {// "https://foo.com/feed1": {lastUpdated: 123, data: []}
      },
      loaded: false
    },
    spocs: {
      spocs_endpoint: "",
      lastUpdated: null,
      data: {// "spocs": {title: "", context: "", items: []},
        // "placement1": {title: "", context: "", items: []},
      },
      loaded: false,
      frequency_caps: [],
      blocked: [],
      placements: []
    }
  },
  Personalization: {
    version: 1,
    lastUpdated: null,
    initialized: false
  },
  Search: {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked, we style
    // the button as if it was a focused search box and show a fake cursor but
    // really focus the awesomebar without the focus styles ("hidden focus").
    fakeFocus: false,
    // Hide the search box after handing off to AwesomeBar and user starts typing.
    hide: false
  }
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case Actions["actionTypes"].INIT:
      return Object.assign({}, prevState, action.data || {}, {
        initialized: true
      });

    default:
      return prevState;
  }
}

function ASRouter(prevState = INITIAL_STATE.ASRouter, action) {
  switch (action.type) {
    case Actions["actionTypes"].AS_ROUTER_INITIALIZED:
      return { ...action.data,
        initialized: true
      };

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
  }); // Then insert them in their specified location

  pinned.forEach((val, index) => {
    if (!val) {
      return;
    }

    let link = Object.assign({}, val, {
      isPinned: true,
      pinIndex: index
    });

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

      return Object.assign({}, prevState, {
        initialized: true,
        rows: action.data.links
      }, action.data.pref ? {
        pref: action.data.pref
      } : {});

    case Actions["actionTypes"].TOP_SITES_PREFS_UPDATED:
      return Object.assign({}, prevState, {
        pref: action.data.pref
      });

    case Actions["actionTypes"].TOP_SITES_EDIT:
      return Object.assign({}, prevState, {
        editForm: {
          index: action.data.index,
          previewResponse: null
        }
      });

    case Actions["actionTypes"].TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, {
        editForm: null
      });

    case Actions["actionTypes"].TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, {
        showSearchShortcutsForm: true
      });

    case Actions["actionTypes"].TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, {
        showSearchShortcutsForm: false
      });

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
          return Object.assign({}, row, {
            screenshot: action.data.screenshot
          });
        }

        return row;
      });
      return hasMatch ? Object.assign({}, prevState, {
        rows: newRows
      }) : prevState;

    case Actions["actionTypes"].PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }

      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const {
            bookmarkGuid,
            bookmarkTitle,
            dateAdded
          } = action.data;
          return Object.assign({}, site, {
            bookmarkGuid,
            bookmarkTitle,
            bookmarkDateCreated: dateAdded
          });
        }

        return site;
      });
      return Object.assign({}, prevState, {
        rows: newRows
      });

    case Actions["actionTypes"].PLACES_BOOKMARKS_REMOVED:
      if (!action.data) {
        return prevState;
      }

      newRows = prevState.rows.map(site => {
        if (site && action.data.urls.includes(site.url)) {
          const newSite = Object.assign({}, site);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;
          return newSite;
        }

        return site;
      });
      return Object.assign({}, prevState, {
        rows: newRows
      });

    case Actions["actionTypes"].PLACES_LINKS_DELETED:
      if (!action.data) {
        return prevState;
      }

      newRows = prevState.rows.filter(site => !action.data.urls.includes(site.url));
      return Object.assign({}, prevState, {
        rows: newRows
      });

    case Actions["actionTypes"].UPDATE_SEARCH_SHORTCUTS:
      return { ...prevState,
        searchShortcuts: action.data.searchShortcuts
      };

    case Actions["actionTypes"].SNIPPETS_PREVIEW_MODE:
      return { ...prevState,
        rows: []
      };

    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case Actions["actionTypes"].DIALOG_OPEN:
      return Object.assign({}, prevState, {
        visible: true,
        data: action.data
      });

    case Actions["actionTypes"].DIALOG_CANCEL:
      return Object.assign({}, prevState, {
        visible: false
      });

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
      return Object.assign({}, prevState, {
        initialized: true,
        values: action.data
      });

    case Actions["actionTypes"].PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, {
        values: newValues
      });

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
      }); // Otherwise, append it

      if (!hasMatch) {
        const initialized = !!(action.data.rows && !!action.data.rows.length);
        const section = Object.assign({
          title: "",
          rows: [],
          enabled: false
        }, action.data, {
          initialized
        });
        newState.push(section);
      }

      return newState;

    case Actions["actionTypes"].SECTION_UPDATE:
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          // If the action is updating rows, we should consider initialized to be true.
          // This can be overridden if initialized is defined in the action.data
          const initialized = action.data.rows ? {
            initialized: true
          } : {}; // Make sure pinned cards stay at their current position when rows are updated.
          // Disabling a section (SECTION_UPDATE with empty rows) does not retain pinned cards.

          if (action.data.rows && !!action.data.rows.length && section.rows.find(card => card.pinned)) {
            const rows = Array.from(action.data.rows);
            section.rows.forEach((card, index) => {
              if (card.pinned) {
                // Only add it if it's not already there.
                if (rows[index].guid !== card.guid) {
                  rows.splice(index, 0, card);
                }
              }
            });
            return Object.assign({}, section, initialized, Object.assign({}, action.data, {
              rows
            }));
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
            return Object.assign({}, section, {
              rows: dedupedRows
            });
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
          return Object.assign({}, section, {
            rows: newRows
          });
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
            const {
              bookmarkGuid,
              bookmarkTitle,
              dateAdded
            } = action.data;
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

    case Actions["actionTypes"].PLACES_BOOKMARKS_REMOVED:
      if (!action.data) {
        return prevState;
      }

      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the bookmark within the rows that is attempted to be removed
          if (action.data.urls.includes(item.url)) {
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

    case Actions["actionTypes"].PLACES_LINKS_DELETED:
      if (!action.data) {
        return prevState;
      }

      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.filter(site => !action.data.urls.includes(site.url))
      }));

    case Actions["actionTypes"].PLACES_LINK_BLOCKED:
      if (!action.data) {
        return prevState;
      }

      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.filter(site => site.url !== action.data.url)
      }));

    case Actions["actionTypes"].DELETE_FROM_POCKET:
    case Actions["actionTypes"].ARCHIVE_FROM_POCKET:
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.filter(site => site.pocket_id !== action.data.pocket_id)
      }));

    case Actions["actionTypes"].SNIPPETS_PREVIEW_MODE:
      return prevState.map(section => ({ ...section,
        rows: []
      }));

    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case Actions["actionTypes"].SNIPPETS_DATA:
      return Object.assign({}, prevState, {
        initialized: true
      }, action.data);

    case Actions["actionTypes"].SNIPPET_BLOCKED:
      return Object.assign({}, prevState, {
        blockList: prevState.blockList.concat(action.data)
      });

    case Actions["actionTypes"].SNIPPETS_BLOCKLIST_CLEARED:
      return Object.assign({}, prevState, {
        blockList: []
      });

    case Actions["actionTypes"].SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;

    default:
      return prevState;
  }
}

function Pocket(prevState = INITIAL_STATE.Pocket, action) {
  switch (action.type) {
    case Actions["actionTypes"].POCKET_WAITING_FOR_SPOC:
      return { ...prevState,
        waitingForSpoc: action.data
      };

    case Actions["actionTypes"].POCKET_LOGGED_IN:
      return { ...prevState,
        isUserLoggedIn: !!action.data
      };

    case Actions["actionTypes"].POCKET_CTA:
      return { ...prevState,
        pocketCta: {
          ctaButton: action.data.cta_button,
          ctaText: action.data.cta_text,
          ctaUrl: action.data.cta_url,
          useCta: action.data.use_cta
        }
      };

    default:
      return prevState;
  }
}

function Personalization(prevState = INITIAL_STATE.Personalization, action) {
  switch (action.type) {
    case Actions["actionTypes"].DISCOVERY_STREAM_PERSONALIZATION_VERSION:
      return { ...prevState,
        version: action.data.version
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED:
      return { ...prevState,
        lastUpdated: action.data.lastUpdated
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_PERSONALIZATION_INIT:
      return { ...prevState,
        initialized: true
      };

    case Actions["actionTypes"].PREF_CHANGED:
      if (action.data.name === PREF_PERSONALIZATION_VERSION) {
        return { ...prevState,
          version: action.data.value
        };
      }

      return prevState;

    default:
      return prevState;
  }
} // eslint-disable-next-line complexity


function DiscoveryStream(prevState = INITIAL_STATE.DiscoveryStream, action) {
  // Return if action data is empty, or spocs or feeds data is not loaded
  const isNotReady = () => !action.data || !prevState.spocs.loaded || !prevState.feeds.loaded;

  const handlePlacements = handleSites => {
    const {
      data,
      placements
    } = prevState.spocs;
    const result = {};

    const forPlacement = placement => {
      const placementSpocs = data[placement.name];

      if (!placementSpocs || !placementSpocs.items || !placementSpocs.items.length) {
        return;
      }

      result[placement.name] = { ...placementSpocs,
        items: handleSites(placementSpocs.items)
      };
    };

    if (!placements || !placements.length) {
      [{
        name: "spocs"
      }].forEach(forPlacement);
    } else {
      placements.forEach(forPlacement);
    }

    return result;
  };

  const nextState = handleSites => ({ ...prevState,
    spocs: { ...prevState.spocs,
      data: handlePlacements(handleSites)
    },
    feeds: { ...prevState.feeds,
      data: Object.keys(prevState.feeds.data).reduce((accumulator, feed_url) => {
        accumulator[feed_url] = {
          data: { ...prevState.feeds.data[feed_url].data,
            recommendations: handleSites(prevState.feeds.data[feed_url].data.recommendations)
          }
        };
        return accumulator;
      }, {})
    }
  });

  switch (action.type) {
    case Actions["actionTypes"].DISCOVERY_STREAM_CONFIG_CHANGE: // Fall through to a separate action is so it doesn't trigger a listener update on init

    case Actions["actionTypes"].DISCOVERY_STREAM_CONFIG_SETUP:
      return { ...prevState,
        config: action.data || {}
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_LAYOUT_UPDATE:
      return { ...prevState,
        lastUpdated: action.data.lastUpdated || null,
        layout: action.data.layout || []
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE:
      return { ...prevState,
        isCollectionDismissible: action.data.value
      };

    case Actions["actionTypes"].HIDE_PRIVACY_INFO:
      return { ...prevState,
        isPrivacyInfoModalVisible: false
      };

    case Actions["actionTypes"].SHOW_PRIVACY_INFO:
      return { ...prevState,
        isPrivacyInfoModalVisible: true
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_LAYOUT_RESET:
      return { ...INITIAL_STATE.DiscoveryStream,
        config: prevState.config
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_FEEDS_UPDATE:
      return { ...prevState,
        feeds: { ...prevState.feeds,
          loaded: true
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_FEED_UPDATE:
      const newData = {};
      newData[action.data.url] = action.data.feed;
      return { ...prevState,
        feeds: { ...prevState.feeds,
          data: { ...prevState.feeds.data,
            ...newData
          }
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_SPOCS_CAPS:
      return { ...prevState,
        spocs: { ...prevState.spocs,
          frequency_caps: [...prevState.spocs.frequency_caps, ...action.data]
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_SPOCS_ENDPOINT:
      return { ...prevState,
        spocs: { ...INITIAL_STATE.DiscoveryStream.spocs,
          spocs_endpoint: action.data.url || INITIAL_STATE.DiscoveryStream.spocs.spocs_endpoint
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_SPOCS_PLACEMENTS:
      return { ...prevState,
        spocs: { ...prevState.spocs,
          placements: action.data.placements || INITIAL_STATE.DiscoveryStream.spocs.placements
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_SPOCS_UPDATE:
      if (action.data) {
        return { ...prevState,
          spocs: { ...prevState.spocs,
            lastUpdated: action.data.lastUpdated,
            data: action.data.spocs,
            loaded: true
          }
        };
      }

      return prevState;

    case Actions["actionTypes"].DISCOVERY_STREAM_SPOC_BLOCKED:
      return { ...prevState,
        spocs: { ...prevState.spocs,
          blocked: [...prevState.spocs.blocked, action.data.url]
        }
      };

    case Actions["actionTypes"].DISCOVERY_STREAM_LINK_BLOCKED:
      return isNotReady() ? prevState : nextState(items => items.filter(item => item.url !== action.data.url));

    case Actions["actionTypes"].PLACES_SAVED_TO_POCKET:
      const addPocketInfo = item => {
        if (item.url === action.data.url) {
          return Object.assign({}, item, {
            open_url: action.data.open_url,
            pocket_id: action.data.pocket_id,
            context_type: "pocket"
          });
        }

        return item;
      };

      return isNotReady() ? prevState : nextState(items => items.map(addPocketInfo));

    case Actions["actionTypes"].DELETE_FROM_POCKET:
    case Actions["actionTypes"].ARCHIVE_FROM_POCKET:
      return isNotReady() ? prevState : nextState(items => items.filter(item => item.pocket_id !== action.data.pocket_id));

    case Actions["actionTypes"].PLACES_BOOKMARK_ADDED:
      const updateBookmarkInfo = item => {
        if (item.url === action.data.url) {
          const {
            bookmarkGuid,
            bookmarkTitle,
            dateAdded
          } = action.data;
          return Object.assign({}, item, {
            bookmarkGuid,
            bookmarkTitle,
            bookmarkDateCreated: dateAdded,
            context_type: "bookmark"
          });
        }

        return item;
      };

      return isNotReady() ? prevState : nextState(items => items.map(updateBookmarkInfo));

    case Actions["actionTypes"].PLACES_BOOKMARKS_REMOVED:
      const removeBookmarkInfo = item => {
        if (action.data.urls.includes(item.url)) {
          const newSite = Object.assign({}, item);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;

          if (!newSite.context_type || newSite.context_type === "bookmark") {
            newSite.context_type = "removedBookmark";
          }

          return newSite;
        }

        return item;
      };

      return isNotReady() ? prevState : nextState(items => items.map(removeBookmarkInfo));

    case Actions["actionTypes"].PREF_CHANGED:
      if (action.data.name === PREF_COLLECTION_DISMISSIBLE) {
        return { ...prevState,
          isCollectionDismissible: action.data.value
        };
      }

      return prevState;

    default:
      return prevState;
  }
}

function Search(prevState = INITIAL_STATE.Search, action) {
  switch (action.type) {
    case Actions["actionTypes"].DISABLE_SEARCH:
      return Object.assign({ ...prevState,
        disable: true
      });

    case Actions["actionTypes"].FAKE_FOCUS_SEARCH:
      return Object.assign({ ...prevState,
        fakeFocus: true
      });

    case Actions["actionTypes"].SHOW_SEARCH:
      return Object.assign({ ...prevState,
        disable: false,
        fakeFocus: false
      });

    default:
      return prevState;
  }
}

var reducers = {
  TopSites,
  App,
  ASRouter,
  Snippets,
  Prefs,
  Dialog,
  Sections,
  Pocket,
  Personalization,
  DiscoveryStream,
  Search
};

/***/ }),
/* 74 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// EXTERNAL MODULE: ./common/Actions.jsm
var Actions = __webpack_require__(1);

// EXTERNAL MODULE: ./content-src/components/A11yLinkButton/A11yLinkButton.jsx
var A11yLinkButton = __webpack_require__(36);

// EXTERNAL MODULE: external "React"
var external_React_ = __webpack_require__(8);
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_);

// EXTERNAL MODULE: ./content-src/components/TopSites/TopSitesConstants.js
var TopSitesConstants = __webpack_require__(56);

// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteFormInput.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class TopSiteFormInput_TopSiteFormInput extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      validationError: this.props.validationError
    };
    this.onChange = this.onChange.bind(this);
    this.onMount = this.onMount.bind(this);
    this.onClearIconPress = this.onClearIconPress.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (nextProps.shouldFocus && !this.props.shouldFocus) {
      this.input.focus();
    }

    if (nextProps.validationError && !this.props.validationError) {
      this.setState({
        validationError: true
      });
    } // If the component is in an error state but the value was cleared by the parent


    if (this.state.validationError && !nextProps.value) {
      this.setState({
        validationError: false
      });
    }
  }

  onClearIconPress(event) {
    // If there is input in the URL or custom image URL fields,
    // and we hit 'enter' while tabbed over the clear icon,
    // we should execute the function to clear the field.
    if (event.key === "Enter") {
      this.props.onClear();
    }
  }

  onChange(ev) {
    if (this.state.validationError) {
      this.setState({
        validationError: false
      });
    }

    this.props.onChange(ev);
  }

  onMount(input) {
    this.input = input;
  }

  renderLoadingOrCloseButton() {
    const showClearButton = this.props.value && this.props.onClear;

    if (this.props.loading) {
      return /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "loading-container"
      }, /*#__PURE__*/external_React_default.a.createElement("div", {
        className: "loading-animation"
      }));
    } else if (showClearButton) {
      return /*#__PURE__*/external_React_default.a.createElement("button", {
        type: "button",
        className: "icon icon-clear-input icon-button-style",
        onClick: this.props.onClear,
        onKeyPress: this.onClearIconPress
      });
    }

    return null;
  }

  render() {
    const {
      typeUrl
    } = this.props;
    const {
      validationError
    } = this.state;
    return /*#__PURE__*/external_React_default.a.createElement("label", null, /*#__PURE__*/external_React_default.a.createElement("span", {
      "data-l10n-id": this.props.titleId
    }), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: `field ${typeUrl ? "url" : ""}${validationError ? " invalid" : ""}`
    }, /*#__PURE__*/external_React_default.a.createElement("input", {
      type: "text",
      value: this.props.value,
      ref: this.onMount,
      onChange: this.onChange,
      "data-l10n-id": this.props.placeholderId // Set focus on error if the url field is valid or when the input is first rendered and is empty
      // eslint-disable-next-line jsx-a11y/no-autofocus
      ,
      autoFocus: this.props.shouldFocus,
      disabled: this.props.loading
    }), this.renderLoadingOrCloseButton(), validationError && /*#__PURE__*/external_React_default.a.createElement("aside", {
      className: "error-tooltip",
      "data-l10n-id": this.props.errorMessageId
    })));
  }

}
TopSiteFormInput_TopSiteFormInput.defaultProps = {
  showClearButton: false,
  value: "",
  validationError: false
};
// EXTERNAL MODULE: ./content-src/components/TopSites/TopSite.jsx
var TopSite = __webpack_require__(58);

// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteForm.jsx
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "TopSiteForm", function() { return TopSiteForm_TopSiteForm; });
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






class TopSiteForm_TopSiteForm extends external_React_default.a.PureComponent {
  constructor(props) {
    super(props);
    const {
      site
    } = props;
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
    this.setState({
      label: event.target.value
    });
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
    this.setState({
      showCustomScreenshotForm: true
    });
  }

  _updateCustomScreenshotInput(customScreenshotUrl) {
    this.setState({
      customScreenshotUrl,
      validationError: false
    });
    this.props.dispatch({
      type: Actions["actionTypes"].PREVIEW_REQUEST_CANCEL
    });
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
      const site = {
        url: this.cleanUrl(this.state.url)
      };
      const {
        index
      } = this.props;

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
        data: {
          site,
          index
        }
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
        data: {
          url: this.cleanUrl(this.state.customScreenshotUrl)
        }
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
    const {
      customScreenshotUrl
    } = this.state;
    return !customScreenshotUrl || this.validateUrl(customScreenshotUrl);
  }

  validateForm() {
    const validate = this.validateUrl(this.state.url) && this.validateCustomScreenshotUrl();

    if (!validate) {
      this.setState({
        validationError: true
      });
    }

    return validate;
  }

  _renderCustomScreenshotInput() {
    const {
      customScreenshotUrl
    } = this.state;
    const requestFailed = this.props.previewResponse === "";
    const validationError = this.state.validationError && !this.validateCustomScreenshotUrl() || requestFailed; // Set focus on error if the url field is valid or when the input is first rendered and is empty

    const shouldFocus = validationError && this.validateUrl(this.state.url) || !customScreenshotUrl;
    const isLoading = this.props.previewResponse === null && customScreenshotUrl && this.props.previewUrl === this.cleanUrl(customScreenshotUrl);

    if (!this.state.showCustomScreenshotForm) {
      return /*#__PURE__*/external_React_default.a.createElement(A11yLinkButton["A11yLinkButton"], {
        onClick: this.onEnableScreenshotUrlForm,
        className: "enable-custom-image-input",
        "data-l10n-id": "newtab-topsites-use-image-link"
      });
    }

    return /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "custom-image-input-container"
    }, /*#__PURE__*/external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, {
      errorMessageId: requestFailed ? "newtab-topsites-image-validation" : "newtab-topsites-url-validation",
      loading: isLoading,
      onChange: this.onCustomScreenshotUrlChange,
      onClear: this.onClearScreenshotInput,
      shouldFocus: shouldFocus,
      typeUrl: true,
      value: customScreenshotUrl,
      validationError: validationError,
      titleId: "newtab-topsites-image-url-label",
      placeholderId: "newtab-topsites-url-input"
    }));
  }

  render() {
    const {
      customScreenshotUrl
    } = this.state;
    const requestFailed = this.props.previewResponse === ""; // For UI purposes, editing without an existing link is "add"

    const showAsAdd = !this.props.site;
    const previous = this.props.site && this.props.site.customScreenshotURL || "";
    const changed = customScreenshotUrl && this.cleanUrl(customScreenshotUrl) !== previous; // Preview mode if changes were made to the custom screenshot URL and no preview was received yet
    // or the request failed

    const previewMode = changed && !this.props.previewResponse;
    const previewLink = Object.assign({}, this.props.site);

    if (this.props.previewResponse) {
      previewLink.screenshot = this.props.previewResponse;
      previewLink.customScreenshotURL = this.props.previewUrl;
    } // Handles the form submit so an enter press performs the correct action


    const onSubmit = previewMode ? this.onPreviewButtonClick : this.onDoneButtonClick; // When the newNewtabExperience is enabled by default, use only shortcut ids.

    const addTopsitesHeaderL10nId = this.props.newNewtabExperienceEnabled || this.props.customizationMenuEnabled ? "newtab-topsites-add-shortcut-header" : "newtab-topsites-add-topsites-header";
    const editTopsitesHeaderL10nId = this.props.newNewtabExperienceEnabled || this.props.customizationMenuEnabled ? "newtab-topsites-edit-shortcut-header" : "newtab-topsites-edit-topsites-header";
    return /*#__PURE__*/external_React_default.a.createElement("form", {
      className: "topsite-form",
      onSubmit: onSubmit
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "form-input-container"
    }, /*#__PURE__*/external_React_default.a.createElement("h3", {
      className: "section-title grey-title",
      "data-l10n-id": showAsAdd ? addTopsitesHeaderL10nId : editTopsitesHeaderL10nId
    }), /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "fields-and-preview"
    }, /*#__PURE__*/external_React_default.a.createElement("div", {
      className: "form-wrapper"
    }, /*#__PURE__*/external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, {
      onChange: this.onLabelChange,
      value: this.state.label,
      titleId: "newtab-topsites-title-label",
      placeholderId: "newtab-topsites-title-input"
    }), /*#__PURE__*/external_React_default.a.createElement(TopSiteFormInput_TopSiteFormInput, {
      onChange: this.onUrlChange,
      shouldFocus: this.state.validationError && !this.validateUrl(this.state.url),
      value: this.state.url,
      onClear: this.onClearUrlClick,
      validationError: this.state.validationError && !this.validateUrl(this.state.url),
      titleId: "newtab-topsites-url-label",
      typeUrl: true,
      placeholderId: "newtab-topsites-url-input",
      errorMessageId: "newtab-topsites-url-validation"
    }), this._renderCustomScreenshotInput()), /*#__PURE__*/external_React_default.a.createElement(TopSite["TopSiteLink"], {
      link: previewLink,
      defaultStyle: requestFailed,
      title: this.state.label,
      newNewtabExperienceEnabled: this.props.newNewtabExperienceEnabled
    }))), /*#__PURE__*/external_React_default.a.createElement("section", {
      className: "actions"
    }, /*#__PURE__*/external_React_default.a.createElement("button", {
      className: "cancel",
      type: "button",
      onClick: this.onCancelButtonClick,
      "data-l10n-id": "newtab-topsites-cancel-button"
    }), previewMode ? /*#__PURE__*/external_React_default.a.createElement("button", {
      className: "done preview",
      type: "submit",
      "data-l10n-id": "newtab-topsites-preview-button"
    }) : /*#__PURE__*/external_React_default.a.createElement("button", {
      className: "done",
      type: "submit",
      "data-l10n-id": showAsAdd ? "newtab-topsites-add-button" : "newtab-topsites-save-button"
    })));
  }

}
TopSiteForm_TopSiteForm.defaultProps = {
  site: null,
  index: -1
};

/***/ })
/******/ ]);