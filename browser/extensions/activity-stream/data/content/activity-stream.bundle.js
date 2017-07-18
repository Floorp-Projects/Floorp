/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
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
/******/ 	// identity function for calling harmony imports with the correct context
/******/ 	__webpack_require__.i = function(value) { return value; };
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
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var MAIN_MESSAGE_TYPE = "ActivityStream:Main";
var CONTENT_MESSAGE_TYPE = "ActivityStream:Content";
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
for (const type of ["BLOCK_URL", "BOOKMARK_URL", "DELETE_BOOKMARK_BY_ID", "DELETE_HISTORY_URL", "DELETE_HISTORY_URL_CONFIRM", "DIALOG_CANCEL", "DIALOG_OPEN", "INIT", "LOCALE_UPDATED", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_UNLOAD", "NEW_TAB_VISIBLE", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "PINNED_SITES_UPDATED", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_CHANGED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINK_BLOCKED", "PLACES_LINK_DELETED", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SET_PREF", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "TOP_SITES_PIN", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "UNINIT"]) {
  actionTypes[type] = type;
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
  ["from", "to", "toTarget", "fromTarget", "skipOrigin"].forEach(o => {
    if (typeof options[o] !== "undefined") {
      meta[o] = options[o];
    } else if (meta[o]) {
      delete meta[o];
    }
  });
  return Object.assign({}, action, { meta });
}

/**
 * SendToMain - Creates a message that will be sent to the Main process.
 *
 * @param  {object} action Any redux action (required)
 * @param  {object} options
 * @param  {string} fromTarget The id of the content port from which the action originated. (optional)
 * @return {object} An action with added .meta properties
 */
function SendToMain(action, fromTarget) {
  return _RouteMessage(action, {
    from: CONTENT_MESSAGE_TYPE,
    to: MAIN_MESSAGE_TYPE,
    fromTarget
  });
}

/**
 * BroadcastToContent - Creates a message that will be sent to ALL content processes.
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
 * SendToContent - Creates a message that will be sent to a particular Content process.
 *
 * @param  {object} action Any redux action (required)
 * @param  {string} target The id of a content port
 * @return {object} An action with added .meta properties
 */
function SendToContent(action, target) {
  if (!target) {
    throw new Error("You must provide a target ID as the second parameter of SendToContent. If you want to send to all content processes, use BroadcastToContent");
  }
  return _RouteMessage(action, {
    from: MAIN_MESSAGE_TYPE,
    to: CONTENT_MESSAGE_TYPE,
    toTarget: target
  });
}

/**
 * UserEvent - A telemetry ping indicating a user action. This should only
 *                   be sent from the UI during a user session.
 *
 * @param  {object} data Fields to include in the ping (source, etc.)
 * @return {object} An SendToMain action
 */
function UserEvent(data) {
  return SendToMain({
    type: actionTypes.TELEMETRY_USER_EVENT,
    data
  });
}

/**
 * UndesiredEvent - A telemetry ping indicating an undesired state.
 *
 * @param  {object} data Fields to include in the ping (value, etc.)
 * @param {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a SendToMain action.
 */
function UndesiredEvent(data) {
  let importContext = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : globalImportContext;

  const action = {
    type: actionTypes.TELEMETRY_UNDESIRED_EVENT,
    data
  };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

/**
 * PerfEvent - A telemetry ping indicating a performance-related event.
 *
 * @param  {object} data Fields to include in the ping (value, etc.)
 * @param {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a SendToMain action.
 */
function PerfEvent(data) {
  let importContext = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : globalImportContext;

  const action = {
    type: actionTypes.TELEMETRY_PERFORMANCE_EVENT,
    data
  };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

function SetPref(name, value) {
  let importContext = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : globalImportContext;

  const action = { type: actionTypes.SET_PREF, data: { name, value } };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

var actionCreators = {
  BroadcastToContent,
  UserEvent,
  UndesiredEvent,
  PerfEvent,
  SendToContent,
  SendToMain,
  SetPref
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
  isSendToContent(action) {
    if (!action.meta) {
      return false;
    }
    if (action.meta.to === CONTENT_MESSAGE_TYPE && action.meta.toTarget) {
      return true;
    }
    return false;
  },
  getPortIdOfSender(action) {
    return action.meta && action.meta.fromTarget || null;
  },
  _RouteMessage
};
module.exports = {
  actionTypes,
  actionCreators,
  actionUtils,
  globalImportContext,
  UI_CODE,
  BACKGROUND_PROCESS,
  MAIN_MESSAGE_TYPE,
  CONTENT_MESSAGE_TYPE
};

/***/ }),
/* 1 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 2 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 3 */
/***/ (function(module, exports) {

module.exports = ReactIntl;

/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/**
 * shortURL - Creates a short version of a link's url, used for display purposes
 *            e.g. {url: http://www.foosite.com, eTLD: "com"}  =>  "foosite"
 *
 * @param  {obj} link A link object
 *         {str} link.url (required)- The url of the link
 *         {str} link.eTLD (required) - The tld of the link
 *               e.g. for https://foo.org, the tld would be "org"
 *               Note that this property is added in various queries for ActivityStream
 *               via Services.eTLD.getPublicSuffix
 *         {str} link.hostname (optional) - The hostname of the url
 *               e.g. for http://www.hello.com/foo/bar, the hostname would be "www.hello.com"
 * @return {str}   A short url
 */
module.exports = function shortURL(link) {
  if (!link.url && !link.hostname) {
    return "";
  }
  const eTLD = link.eTLD;

  const hostname = (link.hostname || new URL(link.url).hostname).replace(/^www\./i, "");

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLDLength = (eTLD || "").length || hostname.match(/\.com$/) && 3;
  const eTLDExtra = eTLDLength > 0 ? -(eTLDLength + 1) : Infinity;
  return hostname.slice(0, eTLDExtra).toLowerCase() || hostname;
};

/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const addLocaleData = _require2.addLocaleData,
      IntlProvider = _require2.IntlProvider;

const TopSites = __webpack_require__(15);
const Search = __webpack_require__(14);
const ConfirmDialog = __webpack_require__(10);
const PreferencesPane = __webpack_require__(13);

// Locales that should be displayed RTL
const RTL_LIST = ["ar", "he", "fa", "ur"];

// Add the locale data for pluralization and relative-time formatting for now,
// this just uses english locale data. We can make this more sophisticated if
// more features are needed.
function addLocaleDataForReactIntl(_ref) {
  let locale = _ref.locale;

  addLocaleData([{ locale, parentLocale: "en" }]);
  document.documentElement.lang = locale;
  document.documentElement.dir = RTL_LIST.indexOf(locale.split("-")[0]) >= 0 ? "rtl" : "ltr";
}

class Base extends React.Component {
  componentDidMount() {
    // Also wait for the preloaded page to show, so the tab's title updates
    addEventListener("visibilitychange", () => this.updateTitle(this.props.App), { once: true });
  }
  componentWillUpdate(_ref2) {
    let App = _ref2.App;

    if (App.locale !== this.props.App.locale) {
      addLocaleDataForReactIntl(App);
      this.updateTitle(App);
    }
  }

  updateTitle(_ref3) {
    let strings = _ref3.strings;

    document.title = strings.newtab_page_title;
  }

  render() {
    const props = this.props;
    var _props$App = props.App;
    const locale = _props$App.locale,
          strings = _props$App.strings,
          initialized = _props$App.initialized;

    const prefs = props.Prefs.values;

    if (!initialized) {
      return null;
    }

    return React.createElement(
      IntlProvider,
      { key: locale, locale: locale, messages: strings },
      React.createElement(
        "div",
        { className: "outer-wrapper" },
        React.createElement(
          "main",
          null,
          prefs.showSearch && React.createElement(Search, null),
          prefs.showTopSites && React.createElement(TopSites, null),
          React.createElement(ConfirmDialog, null)
        ),
        React.createElement(PreferencesPane, null)
      )
    );
  }
}

module.exports = connect(state => ({ App: state.App, Prefs: state.Prefs }))(Base);

/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _require = __webpack_require__(0);

const at = _require.actionTypes;

var _require2 = __webpack_require__(17);

const perfSvc = _require2.perfService;


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

module.exports = class DetectUserSessionStart {
  constructor() {
    let options = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {};

    // Overrides for testing
    this.sendAsyncMessage = options.sendAsyncMessage || window.sendAsyncMessage;
    this.document = options.document || document;
    this._perfService = options.perfService || perfSvc;
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
   *              visibility-change-event time in ms from the UNIX epoch.
   */
  _sendEvent() {
    this._perfService.mark("visibility-change-event");

    let absVisChangeTime = this._perfService.getMostRecentAbsMarkStartByName("visibility-change-event");

    this.sendAsyncMessage("ActivityStream:ContentToMain", {
      type: at.NEW_TAB_VISIBLE,
      data: { absVisibilityChangeTime: absVisChangeTime }
    });
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
};

/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* eslint-env mozilla/frame-script */

var _require = __webpack_require__(18);

const createStore = _require.createStore,
      combineReducers = _require.combineReducers,
      applyMiddleware = _require.applyMiddleware;

var _require2 = __webpack_require__(0);

const au = _require2.actionUtils;


const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";

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
  if (au.isSendToMain(action)) {
    sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  }
  next(action);
};

/**
 * initStore - Create a store and listen for incoming actions
 *
 * @param  {object} reducers An object containing Redux reducers
 * @return {object}          A redux store
 */
module.exports = function initStore(reducers) {
  const store = createStore(mergeStateReducer(combineReducers(reducers)), applyMiddleware(messageMiddleware));

  addMessageListener(INCOMING_MESSAGE_NAME, msg => {
    store.dispatch(msg.data);
  });

  return store;
};

module.exports.MERGE_STORE_ACTION = MERGE_STORE_ACTION;
module.exports.OUTGOING_MESSAGE_NAME = OUTGOING_MESSAGE_NAME;
module.exports.INCOMING_MESSAGE_NAME = INCOMING_MESSAGE_NAME;

/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var _require = __webpack_require__(0);

const at = _require.actionTypes;


const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    // The locale of the browser
    locale: "",
    // Localized strings with defaults
    strings: {},
    // The version of the system-addon
    version: null
  },
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: []
  },
  Prefs: {
    initialized: false,
    values: {}
  },
  Dialog: {
    visible: false,
    data: {}
  }
};

function App() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.App;
  let action = arguments[1];

  switch (action.type) {
    case at.INIT:
      return Object.assign({}, action.data || {}, { initialized: true });
    case at.LOCALE_UPDATED:
      {
        if (!action.data) {
          return prevState;
        }
        var _action$data = action.data;
        let locale = _action$data.locale,
            strings = _action$data.strings;

        return Object.assign({}, prevState, {
          locale,
          strings
        });
      }
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
      delete link.pinTitle;
      delete link.pinIndex;
    }
    return link;
  });

  // Then insert them in their specified location
  pinned.forEach((val, index) => {
    if (!val) {
      return;
    }
    let link = Object.assign({}, val, { isPinned: true, pinIndex: index, pinTitle: val.title });
    if (index > newLinks.length) {
      newLinks[index] = link;
    } else {
      newLinks.splice(index, 0, link);
    }
  });

  return newLinks;
}

function TopSites() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.TopSites;
  let action = arguments[1];

  let hasMatch;
  let newRows;
  let pinned;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, { initialized: true, rows: action.data });
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, { rows: newRows }) : prevState;
    case at.PLACES_BOOKMARK_ADDED:
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          var _action$data2 = action.data;
          const bookmarkGuid = _action$data2.bookmarkGuid,
                bookmarkTitle = _action$data2.bookmarkTitle,
                lastModified = _action$data2.lastModified;

          return Object.assign({}, site, { bookmarkGuid, bookmarkTitle, bookmarkDateCreated: lastModified });
        }
        return site;
      });
      return Object.assign({}, prevState, { rows: newRows });
    case at.PLACES_BOOKMARK_REMOVED:
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
    case at.PLACES_LINK_DELETED:
    case at.PLACES_LINK_BLOCKED:
      newRows = prevState.rows.filter(val => val && val.url !== action.data.url);
      return Object.assign({}, prevState, { rows: newRows });
    case at.PINNED_SITES_UPDATED:
      pinned = action.data;
      newRows = insertPinned(prevState.rows, pinned);
      return Object.assign({}, prevState, { rows: newRows });
    default:
      return prevState;
  }
}

function Dialog() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.Dialog;
  let action = arguments[1];

  switch (action.type) {
    case at.DIALOG_OPEN:
      return Object.assign({}, prevState, { visible: true, data: action.data });
    case at.DIALOG_CANCEL:
      return Object.assign({}, prevState, { visible: false });
    case at.DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.Prefs;
  let action = arguments[1];

  let newValues;
  switch (action.type) {
    case at.PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, { initialized: true, values: action.data });
    case at.PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, { values: newValues });
    default:
      return prevState;
  }
}

var reducers = { TopSites, App, Prefs, Dialog };
module.exports = {
  reducers,
  INITIAL_STATE,
  insertPinned
};

/***/ }),
/* 9 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const FormattedMessage = _require2.FormattedMessage;

var _require3 = __webpack_require__(0);

const actionTypes = _require3.actionTypes,
      ac = _require3.actionCreators;

/**
 * ConfirmDialog component.
 * One primary action button, one cancel button.
 *
 * Content displayed is controlled by `data` prop the component receives.
 * Example:
 * data: {
 *   // Any sort of data needed to be passed around by actions.
 *   payload: site.url,
 *   // Primary button SendToMain action.
 *   action: "DELETE_HISTORY_URL",
 *   // Primary button USerEvent action.
 *   userEvent: "DELETE",
 *   // Array of locale ids to display.
 *   message_body: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
 *   // Text for primary button.
 *   confirm_button_string_id: "menu_action_delete"
 * },
 */

const ConfirmDialog = React.createClass({
  displayName: "ConfirmDialog",

  getDefaultProps() {
    return {
      visible: false,
      data: {}
    };
  },

  _handleCancelBtn() {
    this.props.dispatch({ type: actionTypes.DIALOG_CANCEL });
    this.props.dispatch(ac.UserEvent({ event: actionTypes.DIALOG_CANCEL }));
  },

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  },

  _renderModalMessage() {
    const message_body = this.props.data.body_string_id;

    if (!message_body) {
      return null;
    }

    return React.createElement(
      "span",
      null,
      message_body.map(msg => React.createElement(
        "p",
        { key: msg },
        React.createElement(FormattedMessage, { id: msg })
      ))
    );
  },

  render() {
    if (!this.props.visible) {
      return null;
    }

    return React.createElement(
      "div",
      { className: "confirmation-dialog" },
      React.createElement("div", { className: "modal-overlay", onClick: this._handleCancelBtn }),
      React.createElement(
        "div",
        { className: "modal", ref: "modal" },
        React.createElement(
          "section",
          { className: "modal-message" },
          this._renderModalMessage()
        ),
        React.createElement(
          "section",
          { className: "actions" },
          React.createElement(
            "button",
            { ref: "cancelButton", onClick: this._handleCancelBtn },
            React.createElement(FormattedMessage, { id: "topsites_form_cancel_button" })
          ),
          React.createElement(
            "button",
            { ref: "confirmButton", className: "done", onClick: this._handleConfirmBtn },
            React.createElement(FormattedMessage, { id: this.props.data.confirm_button_string_id })
          )
        )
      )
    );
  }
});

module.exports = connect(state => state.Dialog)(ConfirmDialog);
module.exports._unconnected = ConfirmDialog;
module.exports.Dialog = ConfirmDialog;

/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

class ContextMenu extends React.Component {
  constructor(props) {
    super(props);
    this.hideContext = this.hideContext.bind(this);
  }
  hideContext() {
    this.props.onUpdate(false);
  }
  componentWillMount() {
    this.hideContext();
  }
  componentDidUpdate(prevProps) {
    if (this.props.visible && !prevProps.visible) {
      setTimeout(() => {
        window.addEventListener("click", this.hideContext);
      }, 0);
    }
    if (!this.props.visible && prevProps.visible) {
      window.removeEventListener("click", this.hideContext);
    }
  }
  componentDidUnmount() {
    window.removeEventListener("click", this.hideContext);
  }
  onKeyDown(event, option) {
    switch (event.key) {
      case "Tab":
        // tab goes down in context menu, shift + tab goes up in context menu
        // if we're on the last item, one more tab will close the context menu
        // similarly, if we're on the first item, one more shift + tab will close it
        if (event.shiftKey && option.first || !event.shiftKey && option.last) {
          this.hideContext();
        }
        break;
      case "Enter":
        this.hideContext();
        option.onClick();
        break;
    }
  }
  render() {
    return React.createElement(
      "span",
      { hidden: !this.props.visible, className: "context-menu" },
      React.createElement(
        "ul",
        { role: "menu", className: "context-menu-list" },
        this.props.options.map((option, i) => {
          if (option.type === "separator") {
            return React.createElement("li", { key: i, className: "separator" });
          }
          return React.createElement(
            "li",
            { role: "menuitem", className: "context-menu-item", key: i },
            React.createElement(
              "a",
              { tabIndex: "0",
                onKeyDown: e => this.onKeyDown(e, option),
                onClick: () => {
                  this.hideContext();
                  option.onClick();
                } },
              option.icon && React.createElement("span", { className: `icon icon-spacer icon-${option.icon}` }),
              option.label
            )
          );
        })
      )
    );
  }
}

module.exports = ContextMenu;

/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

var _require = __webpack_require__(3);

const injectIntl = _require.injectIntl;

const ContextMenu = __webpack_require__(11);

var _require2 = __webpack_require__(0);

const ac = _require2.actionCreators;

const linkMenuOptions = __webpack_require__(16);
const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow"];

class LinkMenu extends React.Component {
  getOptions() {
    const props = this.props;
    const site = props.site,
          index = props.index,
          source = props.source;

    // Handle special case of default site

    const propOptions = !site.isDefault ? props.options : DEFAULT_SITE_MENU_OPTIONS;

    const options = propOptions.map(o => linkMenuOptions[o](site, index)).map(option => {
      const action = option.action,
            id = option.id,
            type = option.type,
            userEvent = option.userEvent;

      if (!type && id) {
        option.label = props.intl.formatMessage(option);
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            props.dispatch(ac.UserEvent({
              event: userEvent,
              source,
              action_position: index
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
    return React.createElement(ContextMenu, {
      visible: this.props.visible,
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}

module.exports = injectIntl(LinkMenu);
module.exports._unconnected = LinkMenu;

/***/ }),
/* 13 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const injectIntl = _require2.injectIntl,
      FormattedMessage = _require2.FormattedMessage;

var _require3 = __webpack_require__(0);

const ac = _require3.actionCreators;


const PreferencesInput = props => React.createElement(
  "section",
  null,
  React.createElement("input", { type: "checkbox", id: props.prefName, name: props.prefName, checked: props.value, onChange: props.onChange, className: props.className }),
  React.createElement(
    "label",
    { htmlFor: props.prefName },
    React.createElement(FormattedMessage, { id: props.titleStringId })
  ),
  props.descStringId && React.createElement(
    "p",
    { className: "prefs-input-description" },
    React.createElement(FormattedMessage, { id: props.descStringId })
  )
);

class PreferencesPane extends React.Component {
  constructor(props) {
    super(props);
    this.state = { visible: false };
    this.handleClickOutside = this.handleClickOutside.bind(this);
    this.handleChange = this.handleChange.bind(this);
    this.togglePane = this.togglePane.bind(this);
  }
  componentDidMount() {
    document.addEventListener("click", this.handleClickOutside);
  }
  componentWillUnmount() {
    document.removeEventListener("click", this.handleClickOutside);
  }
  handleClickOutside(event) {
    // if we are showing the sidebar and there is a click outside, close it.
    if (this.state.visible && !this.refs.wrapper.contains(event.target)) {
      this.togglePane();
    }
  }
  handleChange(event) {
    const target = event.target;
    this.props.dispatch(ac.SetPref(target.name, target.checked));
  }
  togglePane() {
    this.setState({ visible: !this.state.visible });
    const event = this.state.visible ? "CLOSE_NEWTAB_PREFS" : "OPEN_NEWTAB_PREFS";
    this.props.dispatch(ac.UserEvent({ event }));
  }
  render() {
    const props = this.props;
    const prefs = props.Prefs.values;
    const isVisible = this.state.visible;
    return React.createElement(
      "div",
      { className: "prefs-pane-wrapper", ref: "wrapper" },
      React.createElement(
        "div",
        { className: "prefs-pane-button" },
        React.createElement("button", {
          className: `prefs-button icon ${isVisible ? "icon-dismiss" : "icon-settings"}`,
          title: props.intl.formatMessage({ id: isVisible ? "settings_pane_done_button" : "settings_pane_button_label" }),
          onClick: this.togglePane })
      ),
      React.createElement(
        "div",
        { className: "prefs-pane" },
        React.createElement(
          "div",
          { className: `sidebar ${isVisible ? "" : "hidden"}` },
          React.createElement(
            "div",
            { className: "prefs-modal-inner-wrapper" },
            React.createElement(
              "h1",
              null,
              React.createElement(FormattedMessage, { id: "settings_pane_header" })
            ),
            React.createElement(
              "p",
              null,
              React.createElement(FormattedMessage, { id: "settings_pane_body" })
            ),
            React.createElement(PreferencesInput, { className: "showSearch", prefName: "showSearch", value: prefs.showSearch, onChange: this.handleChange,
              titleStringId: "settings_pane_search_header", descStringId: "settings_pane_search_body" }),
            React.createElement(PreferencesInput, { className: "showTopSites", prefName: "showTopSites", value: prefs.showTopSites, onChange: this.handleChange,
              titleStringId: "settings_pane_topsites_header", descStringId: "settings_pane_topsites_body" })
          ),
          React.createElement(
            "section",
            { className: "actions" },
            React.createElement(
              "button",
              { className: "done", onClick: this.togglePane },
              React.createElement(FormattedMessage, { id: "settings_pane_done_button" })
            )
          )
        )
      )
    );
  }
}

module.exports = connect(state => ({ Prefs: state.Prefs }))(injectIntl(PreferencesPane));
module.exports.PreferencesPane = PreferencesPane;
module.exports.PreferencesInput = PreferencesInput;

/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals ContentSearchUIController */


const React = __webpack_require__(1);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const FormattedMessage = _require2.FormattedMessage,
      injectIntl = _require2.injectIntl;

var _require3 = __webpack_require__(0);

const ac = _require3.actionCreators;


class Search extends React.Component {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
  }

  handleEvent(event) {
    // Also track search events with our own telemetry
    if (event.detail.type === "Search") {
      this.props.dispatch(ac.UserEvent({ event: "SEARCH" }));
    }
  }
  onClick(event) {
    this.controller.search(event);
  }
  onInputMount(input) {
    if (input) {
      // The first "newtab" parameter here is called the "healthReportKey" and needs
      // to be "newtab" so that BrowserUsageTelemetry.jsm knows to handle events with
      // this name, and can add the appropriate telemetry probes for search. Without the
      // correct name, certain tests like browser_UsageTelemetry_content.js will fail (See
      // github ticket #2348 for more details)
      this.controller = new ContentSearchUIController(input, input.parentNode, "newtab", "newtab");
      addEventListener("ContentSearchClient", this);
    } else {
      this.controller = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  /*
   * Do not change the ID on the input field, as legacy newtab code
   * specifically looks for the id 'newtab-search-text' on input fields
   * in order to execute searches in various tests
   */
  render() {
    return React.createElement(
      "form",
      { className: "search-wrapper" },
      React.createElement(
        "label",
        { htmlFor: "newtab-search-text", className: "search-label" },
        React.createElement(
          "span",
          { className: "sr-only" },
          React.createElement(FormattedMessage, { id: "search_web_placeholder" })
        )
      ),
      React.createElement("input", {
        id: "newtab-search-text",
        maxLength: "256",
        placeholder: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        ref: this.onInputMount,
        title: this.props.intl.formatMessage({ id: "search_web_placeholder" }),
        type: "search" }),
      React.createElement(
        "button",
        {
          className: "search-button",
          onClick: this.onClick,
          title: this.props.intl.formatMessage({ id: "search_button" }) },
        React.createElement(
          "span",
          { className: "sr-only" },
          React.createElement(FormattedMessage, { id: "search_button" })
        )
      )
    );
  }
}

module.exports = connect()(injectIntl(Search));
module.exports._unconnected = Search;

/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const FormattedMessage = _require2.FormattedMessage;

const shortURL = __webpack_require__(4);
const LinkMenu = __webpack_require__(12);

var _require3 = __webpack_require__(0);

const ac = _require3.actionCreators;

const TOP_SITES_SOURCE = "TOP_SITES";
const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];

class TopSite extends React.Component {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false, activeTile: null };
  }
  toggleContextMenu(event, index) {
    this.setState({ showContextMenu: true, activeTile: index });
  }
  trackClick() {
    this.props.dispatch(ac.UserEvent({
      event: "CLICK",
      source: TOP_SITES_SOURCE,
      action_position: this.props.index
    }));
  }
  render() {
    var _props = this.props;
    const link = _props.link,
          index = _props.index,
          dispatch = _props.dispatch;

    const isContextMenuOpen = this.state.showContextMenu && this.state.activeTile === index;
    const title = link.pinTitle || shortURL(link);
    const screenshotClassName = `screenshot${link.screenshot ? " active" : ""}`;
    const topSiteOuterClassName = `top-site-outer${isContextMenuOpen ? " active" : ""}`;
    const style = { backgroundImage: link.screenshot ? `url(${link.screenshot})` : "none" };
    return React.createElement(
      "li",
      { className: topSiteOuterClassName, key: link.url },
      React.createElement(
        "a",
        { onClick: () => this.trackClick(), href: link.url },
        React.createElement(
          "div",
          { className: "tile", "aria-hidden": true },
          React.createElement(
            "span",
            { className: "letter-fallback" },
            title[0]
          ),
          React.createElement("div", { className: screenshotClassName, style: style })
        ),
        React.createElement(
          "div",
          { className: `title ${link.isPinned ? "pinned" : ""}` },
          link.isPinned && React.createElement("div", { className: "icon icon-pin-small" }),
          React.createElement(
            "span",
            null,
            title
          )
        )
      ),
      React.createElement(
        "button",
        { className: "context-menu-button",
          onClick: e => {
            e.preventDefault();
            this.toggleContextMenu(e, index);
          } },
        React.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${title}`
        )
      ),
      React.createElement(LinkMenu, {
        dispatch: dispatch,
        visible: isContextMenuOpen,
        onUpdate: val => this.setState({ showContextMenu: val }),
        site: link,
        index: index,
        source: TOP_SITES_SOURCE,
        options: TOP_SITES_CONTEXT_MENU_OPTIONS })
    );
  }
}

const TopSites = props => React.createElement(
  "section",
  null,
  React.createElement(
    "h3",
    { className: "section-title" },
    React.createElement(FormattedMessage, { id: "header_top_sites" })
  ),
  React.createElement(
    "ul",
    { className: "top-sites-list" },
    props.TopSites.rows.map((link, index) => link && React.createElement(TopSite, {
      key: link.url,
      dispatch: props.dispatch,
      link: link,
      index: index }))
  )
);

module.exports = connect(state => ({ TopSites: state.TopSites }))(TopSites);
module.exports._unconnected = TopSites;
module.exports.TopSite = TopSite;

/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _require = __webpack_require__(0);

const at = _require.actionTypes,
      ac = _require.actionCreators;

const shortURL = __webpack_require__(4);

/**
 * List of functions that return items that can be included as menu options in a
 * LinkMenu. All functions take the site as the first parameter, and optionally
 * the index of the site.
 */
module.exports = {
  Separator: () => ({ type: "separator" }),
  RemoveBookmark: site => ({
    id: "menu_action_remove_bookmark",
    icon: "bookmark-remove",
    action: ac.SendToMain({
      type: at.DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "menu_action_bookmark",
    icon: "bookmark",
    action: ac.SendToMain({
      type: at.BOOKMARK_URL,
      data: site.url
    }),
    userEvent: "BOOKMARK_ADD"
  }),
  OpenInNewWindow: site => ({
    id: "menu_action_open_new_window",
    icon: "new-window",
    action: ac.SendToMain({
      type: at.OPEN_NEW_WINDOW,
      data: { url: site.url }
    }),
    userEvent: "OPEN_NEW_WINDOW"
  }),
  OpenInPrivateWindow: site => ({
    id: "menu_action_open_private_window",
    icon: "new-window-private",
    action: ac.SendToMain({
      type: at.OPEN_PRIVATE_WINDOW,
      data: { url: site.url }
    }),
    userEvent: "OPEN_PRIVATE_WINDOW"
  }),
  BlockUrl: site => ({
    id: "menu_action_dismiss",
    icon: "dismiss",
    action: ac.SendToMain({
      type: at.BLOCK_URL,
      data: site.url
    }),
    userEvent: "BLOCK"
  }),
  DeleteUrl: site => ({
    id: "menu_action_delete",
    icon: "delete",
    action: {
      type: at.DIALOG_OPEN,
      data: {
        onConfirm: [ac.SendToMain({ type: at.DELETE_HISTORY_URL, data: site.url }), ac.UserEvent({ event: "DELETE" })],
        body_string_id: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
        confirm_button_string_id: "menu_action_delete"
      }
    },
    userEvent: "DIALOG_OPEN"
  }),
  PinTopSite: (site, index) => ({
    id: "menu_action_pin",
    icon: "pin",
    action: ac.SendToMain({
      type: at.TOP_SITES_PIN,
      data: { site: { url: site.url, title: shortURL(site) }, index }
    }),
    userEvent: "PIN"
  }),
  UnpinTopSite: site => ({
    id: "menu_action_unpin",
    icon: "unpin",
    action: ac.SendToMain({
      type: at.TOP_SITES_UNPIN,
      data: { site: { url: site.url } }
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: site => ({
    id: "menu_action_save_to_pocket",
    icon: "pocket",
    action: ac.SendToMain({
      type: at.SAVE_TO_POCKET,
      data: { site: { url: site.url, title: site.title } }
    }),
    userEvent: "SAVE_TO_POCKET"
  })
};

module.exports.CheckBookmark = site => site.bookmarkGuid ? module.exports.RemoveBookmark(site) : module.exports.AddBookmark(site);
module.exports.CheckPinTopSite = (site, index) => site.isPinned ? module.exports.UnpinTopSite(site) : module.exports.PinTopSite(site, index);

/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals Services */


let usablePerfObj;

let Cu;
const isRunningInChrome = typeof Window === "undefined";

/* istanbul ignore if */
if (isRunningInChrome) {
  Cu = Components.utils;
} else {
  Cu = { import() {} };
}

Cu.import("resource://gre/modules/Services.jsm");

/* istanbul ignore if */
if (isRunningInChrome) {
  // Borrow the high-resolution timer from the hidden window....
  usablePerfObj = Services.appShell.hiddenDOMWindow.performance;
} else {
  // we must be running in content space
  usablePerfObj = performance;
}

var _PerfService = function _PerfService(options) {
  // For testing, so that we can use a fake Window.performance object with
  // known state.
  if (options && options.performanceObj) {
    this._perf = options.performanceObj;
  } else {
    this._perf = usablePerfObj;
  }
};

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
   * @return {Number} A double of milliseconds with a precision of 0.5us.
   */
  get timeOrigin() {
    return this._perf.timeOrigin;
  },

  /**
   * This returns the startTime from the most recen!t performance.mark()
   * with the given name.
   *
   * @param  {String} name  the name to lookup the start time for
   *
   * @return {Number}       the returned start time, as a DOMHighResTimeStamp
   *
   * @throws {Error}        "No Marks with the name ..." if none are available
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
module.exports = {
  _PerfService,
  perfService
};

/***/ }),
/* 18 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(1);
const ReactDOM = __webpack_require__(9);
const Base = __webpack_require__(5);

var _require = __webpack_require__(2);

const Provider = _require.Provider;

const initStore = __webpack_require__(7);

var _require2 = __webpack_require__(8);

const reducers = _require2.reducers;

const DetectUserSessionStart = __webpack_require__(6);

new DetectUserSessionStart().sendEventOrAddListener();

const store = initStore(reducers);

ReactDOM.render(React.createElement(
  Provider,
  { store: store },
  React.createElement(Base, null)
), document.getElementById("root"));

/***/ })
/******/ ]);