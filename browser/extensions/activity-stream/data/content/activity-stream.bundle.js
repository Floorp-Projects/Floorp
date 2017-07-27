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
/******/ 	return __webpack_require__(__webpack_require__.s = 26);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 1 */
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
for (const type of ["BLOCK_URL", "BOOKMARK_URL", "DELETE_BOOKMARK_BY_ID", "DELETE_HISTORY_URL", "DELETE_HISTORY_URL_CONFIRM", "DIALOG_CANCEL", "DIALOG_OPEN", "FEED_INIT", "INIT", "LOCALE_UPDATED", "MIGRATION_CANCEL", "MIGRATION_START", "NEW_TAB_INIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_UNLOAD", "OPEN_LINK", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "PINNED_SITES_UPDATED", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_CHANGED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINK_BLOCKED", "PLACES_LINK_DELETED", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "SAVE_SESSION_PERF_DATA", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SECTION_DEREGISTER", "SECTION_REGISTER", "SECTION_ROWS_UPDATE", "SET_PREF", "SNIPPETS_DATA", "SNIPPETS_RESET", "SYSTEM_TICK", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "TOP_SITES_PIN", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "UNINIT"]) {
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
/* 2 */
/***/ (function(module, exports) {

module.exports = ReactIntl;

/***/ }),
/* 3 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

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
 *         {str} link.title (optional) - The title of the link
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
  // If URL and hostname are not present fallback to page title.
  return hostname.slice(0, eTLDExtra).toLowerCase() || hostname || link.title || link.url;
};

/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(2);

const injectIntl = _require.injectIntl;

const ContextMenu = __webpack_require__(16);

var _require2 = __webpack_require__(1);

const ac = _require2.actionCreators;

const linkMenuOptions = __webpack_require__(23);
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
/* 6 */
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
   * Returns the "absolute" version of performance.now(), i.e. one that
   * based on the timeOrigin of the XUL hiddenwindow.
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
module.exports = {
  _PerfService,
  perfService
};

/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const addLocaleData = _require2.addLocaleData,
      IntlProvider = _require2.IntlProvider;

const TopSites = __webpack_require__(21);
const Search = __webpack_require__(19);
const ConfirmDialog = __webpack_require__(15);
const ManualMigration = __webpack_require__(17);
const PreferencesPane = __webpack_require__(18);
const Sections = __webpack_require__(20);

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
          !prefs.migrationExpired && React.createElement(ManualMigration, null),
          prefs.showTopSites && React.createElement(TopSites, null),
          React.createElement(Sections, null),
          React.createElement(ConfirmDialog, null)
        ),
        React.createElement(PreferencesPane, null)
      )
    );
  }
}

module.exports = connect(state => ({ App: state.App, Prefs: state.Prefs }))(Base);

/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _require = __webpack_require__(1);

const at = _require.actionTypes;

var _require2 = __webpack_require__(6);

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
   *              visibility_event_rcvd_ts time in ms from the UNIX epoch.
   */
  _sendEvent() {
    this._perfService.mark("visibility_event_rcvd_ts");

    try {
      let visibility_event_rcvd_ts = this._perfService.getMostRecentAbsMarkStartByName("visibility_event_rcvd_ts");

      this.sendAsyncMessage("ActivityStream:ContentToMain", {
        type: at.SAVE_SESSION_PERF_DATA,
        data: { visibility_event_rcvd_ts }
      });
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
};

/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* eslint-env mozilla/frame-script */

var _require = __webpack_require__(25);

const createStore = _require.createStore,
      combineReducers = _require.combineReducers,
      applyMiddleware = _require.applyMiddleware;

var _require2 = __webpack_require__(1);

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
    try {
      store.dispatch(msg.data);
    } catch (ex) {
      console.error("Content msg:", msg, "Dispatch error: ", ex); // eslint-disable-line no-console
      dump(`Content msg: ${JSON.stringify(msg)}\nDispatch error: ${ex}\n${ex.stack}`);
    }
  });

  return store;
};

module.exports.MERGE_STORE_ACTION = MERGE_STORE_ACTION;
module.exports.OUTGOING_MESSAGE_NAME = OUTGOING_MESSAGE_NAME;
module.exports.INCOMING_MESSAGE_NAME = INCOMING_MESSAGE_NAME;

/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(global) {

const DATABASE_NAME = "snippets_db";
const DATABASE_VERSION = 1;
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
const SNIPPETS_UPDATE_INTERVAL_MS = 14400000; // 4 hours.

/**
 * SnippetsMap - A utility for cacheing values related to the snippet. It has
 *               the same interface as a Map, but is optionally backed by
 *               indexedDB for persistent storage.
 *               Call .connect() to open a database connection and restore any
 *               previously cached data, if necessary.
 *
 */
class SnippetsMap extends Map {
  constructor() {
    super(...arguments);
    this._db = null;
  }

  set(key, value) {
    super.set(key, value);
    return this._dbTransaction(db => db.put(value, key));
  }

  delete(key, value) {
    super.delete(key);
    return this._dbTransaction(db => db.delete(key));
  }

  clear() {
    super.clear();
    return this._dbTransaction(db => db.clear());
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
          this.set(cursor.key, cursor.value);
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
  constructor() {
    // Initialize the Snippets Map and attaches it to a global so that
    // the snippet payload can interact with it.
    global.gSnippetsMap = new SnippetsMap();
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
        // TODO: timeout?
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

  _showDefaultSnippets() {
    // TODO
  }

  _showRemoteSnippets() {
    const snippetsEl = document.getElementById(this.elementId);
    const containerEl = document.getElementById(this.containerElementId);
    const payload = this.snippetsMap.get("snippets");

    if (!snippetsEl) {
      throw new Error(`No element was found with id '${this.elementId}'.`);
    }

    // This could happen if fetching failed
    if (!payload) {
      throw new Error("No remote snippets were found in gSnippetsMap.");
    }

    // Note that injecting snippets can throw if they're invalid XML.
    snippetsEl.innerHTML = payload;

    // Scripts injected by innerHTML are inactive, so we have to relocate them
    // through DOM manipulation to activate their contents.
    for (const scriptEl of snippetsEl.getElementsByTagName("script")) {
      const relocatedScript = document.createElement("script");
      relocatedScript.text = scriptEl.text;
      scriptEl.parentNode.replaceChild(relocatedScript, scriptEl);
    }

    // Unhide the container if everything went OK
    if (containerEl) {
      containerEl.style.display = "block";
    }
  }

  /**
   * init - Fetch the snippet payload and show snippets
   *
   * @param  {obj} options
   * @param  {str} options.appData.snippetsURL  The URL from which we fetch snippets
   * @param  {int} options.appData.version  The current snippets version
   * @param  {str} options.elementId  The id of the element in which to inject snippets
   * @param  {str} options.containerElementId  The id of the element of element containing the snippets element
   * @param  {bool} options.connect  Should gSnippetsMap connect to indexedDB?
   */
  async init(options) {
    Object.assign(this, {
      appData: {},
      elementId: "snippets",
      containerElementId: "snippets-container",
      connect: true
    }, options);

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
      this.snippetsMap.set(`appData.${key}`, this.appData[key]);
    }

    // Refresh snippets, if enough time has passed.
    await this._refreshSnippets();

    // Try showing remote snippets, falling back to defaults if necessary.
    try {
      this._showRemoteSnippets();
    } catch (e) {
      this._showDefaultSnippets(e);
    }
  }
}

module.exports.SnippetsMap = SnippetsMap;
module.exports.SnippetsProvider = SnippetsProvider;
module.exports.SNIPPETS_UPDATE_INTERVAL_MS = SNIPPETS_UPDATE_INTERVAL_MS;
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(24)))

/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var _require = __webpack_require__(1);

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
  Snippets: { initialized: false },
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
  },
  Sections: []
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
      if (!action.data) {
        return prevState;
      }
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

function Sections() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.Sections;
  let action = arguments[1];

  let hasMatch;
  let newState;
  switch (action.type) {
    case at.SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case at.SECTION_REGISTER:
      // If section exists in prevState, update it
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          hasMatch = true;
          return Object.assign({}, section, action.data);
        }
        return section;
      });
      // If section doesn't exist in prevState, create a new section object and
      // append it to the sections state
      if (!hasMatch) {
        const initialized = action.data.rows && action.data.rows.length > 0;
        newState.push(Object.assign({ title: "", initialized, rows: [] }, action.data));
      }
      return newState;
    case at.SECTION_ROWS_UPDATE:
      return prevState.map(section => {
        if (section && section.id === action.data.id) {
          return Object.assign({}, section, action.data);
        }
        return section;
      });
    case at.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the item within the rows that is attempted to be bookmarked
          if (item.url === action.data.url) {
            var _action$data3 = action.data;
            const bookmarkGuid = _action$data3.bookmarkGuid,
                  bookmarkTitle = _action$data3.bookmarkTitle,
                  lastModified = _action$data3.lastModified;

            Object.assign(item, { bookmarkGuid, bookmarkTitle, bookmarkDateCreated: lastModified });
          }
          return item;
        })
      }));
    case at.PLACES_BOOKMARK_REMOVED:
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
            return newSite;
          }
          return item;
        })
      }));
    case at.PLACES_LINK_DELETED:
    case at.PLACES_LINK_BLOCKED:
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.url !== action.data.url) }));
    default:
      return prevState;
  }
}

function Snippets() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.Snippets;
  let action = arguments[1];

  switch (action.type) {
    case at.SNIPPETS_DATA:
      return Object.assign({}, prevState, { initialized: true }, action.data);
    case at.SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

var reducers = { TopSites, App, Snippets, Prefs, Dialog, Sections };
module.exports = {
  reducers,
  INITIAL_STATE,
  insertPinned
};

/***/ }),
/* 12 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 13 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);
const LinkMenu = __webpack_require__(5);
const shortURL = __webpack_require__(4);

var _require = __webpack_require__(2);

const FormattedMessage = _require.FormattedMessage;

const cardContextTypes = __webpack_require__(14);

var _require2 = __webpack_require__(1);

const ac = _require2.actionCreators,
      at = _require2.actionTypes;

/**
 * Card component.
 * Cards are found within a Section component and contain information about a link such
 * as preview image, page title, page description, and some context about if the page
 * was visited, bookmarked, trending etc...
 * Each Section can make an unordered list of Cards which will create one instane of
 * this class. Each card will then get a context menu which reflects the actions that
 * can be done on this Card.
 */

class Card extends React.Component {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false, activeCard: null };
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }
  onMenuButtonClick(event) {
    event.preventDefault();
    this.setState({
      activeCard: this.props.index,
      showContextMenu: true
    });
  }
  onLinkClick(event) {
    event.preventDefault();
    this.props.dispatch(ac.SendToMain({ type: at.OPEN_LINK, data: this.props.link }));
    this.props.dispatch(ac.UserEvent({
      event: "CLICK",
      source: this.props.eventSource,
      action_position: this.props.index
    }));
  }
  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }
  render() {
    var _props = this.props;
    const index = _props.index,
          link = _props.link,
          dispatch = _props.dispatch,
          contextMenuOptions = _props.contextMenuOptions;

    const isContextMenuOpen = this.state.showContextMenu && this.state.activeCard === index;
    const hostname = shortURL(link);
    var _cardContextTypes$lin = cardContextTypes[link.type];
    const icon = _cardContextTypes$lin.icon,
          intlID = _cardContextTypes$lin.intlID;


    return React.createElement(
      "li",
      { className: `card-outer${isContextMenuOpen ? " active" : ""}` },
      React.createElement(
        "a",
        { href: link.url, onClick: this.onLinkClick },
        React.createElement(
          "div",
          { className: "card" },
          link.image && React.createElement("div", { className: "card-preview-image", style: { backgroundImage: `url(${link.image})` } }),
          React.createElement(
            "div",
            { className: "card-details" },
            React.createElement(
              "div",
              { className: "card-host-name" },
              " ",
              hostname,
              " "
            ),
            React.createElement(
              "div",
              { className: `card-text${link.image ? "" : " full-height"}` },
              React.createElement(
                "h4",
                { className: "card-title", dir: "auto" },
                " ",
                link.title,
                " "
              ),
              React.createElement(
                "p",
                { className: "card-description", dir: "auto" },
                " ",
                link.description,
                " "
              )
            ),
            React.createElement(
              "div",
              { className: "card-context" },
              React.createElement("span", { className: `card-context-icon icon icon-${icon}` }),
              React.createElement(
                "div",
                { className: "card-context-label" },
                React.createElement(FormattedMessage, { id: intlID, defaultMessage: "Visited" })
              )
            )
          )
        )
      ),
      React.createElement(
        "button",
        { className: "context-menu-button",
          onClick: this.onMenuButtonClick },
        React.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${link.title}`
        )
      ),
      React.createElement(LinkMenu, {
        dispatch: dispatch,
        index: index,
        onUpdate: this.onMenuUpdate,
        options: link.context_menu_options || contextMenuOptions,
        site: link,
        visible: isContextMenuOpen })
    );
  }
}
module.exports = Card;

/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


module.exports = {
  history: {
    intlID: "type_label_visited",
    icon: "historyItem"
  },
  bookmark: {
    intlID: "type_label_bookmarked",
    icon: "bookmark"
  },
  trending: {
    intlID: "type_label_recommended",
    icon: "trending"
  },
  now: {
    intlID: "type_label_now",
    icon: "now"
  }
};

/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const FormattedMessage = _require2.FormattedMessage;

var _require3 = __webpack_require__(1);

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
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

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
  componentWillUnmount() {
    window.removeEventListener("click", this.hideContext);
  }
  render() {
    return React.createElement(
      "span",
      { hidden: !this.props.visible, className: "context-menu" },
      React.createElement(
        "ul",
        { role: "menu", className: "context-menu-list" },
        this.props.options.map((option, i) => option.type === "separator" ? React.createElement("li", { key: i, className: "separator" }) : React.createElement(ContextMenuItem, { key: i, option: option, hideContext: this.hideContext }))
      )
    );
  }
}

class ContextMenuItem extends React.Component {
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
    const option = this.props.option;

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
    const option = this.props.option;

    return React.createElement(
      "li",
      { role: "menuitem", className: "context-menu-item" },
      React.createElement(
        "a",
        { onClick: this.onClick, onKeyDown: this.onKeyDown, tabIndex: "0" },
        option.icon && React.createElement("span", { className: `icon icon-spacer icon-${option.icon}` }),
        option.label
      )
    );
  }
}

module.exports = ContextMenu;
module.exports.ContextMenu = ContextMenu;
module.exports.ContextMenuItem = ContextMenuItem;

/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const FormattedMessage = _require2.FormattedMessage;

var _require3 = __webpack_require__(1);

const at = _require3.actionTypes,
      ac = _require3.actionCreators;

/**
 * Manual migration component used to start the profile import wizard.
 * Message is presented temporarily and will go away if:
 * 1.  User clicks "No Thanks"
 * 2.  User completed the data import
 * 3.  After 3 active days
 * 4.  User clicks "Cancel" on the import wizard (currently not implemented).
 */

class ManualMigration extends React.Component {
  constructor(props) {
    super(props);
    this.onLaunchTour = this.onLaunchTour.bind(this);
    this.onCancelTour = this.onCancelTour.bind(this);
  }
  onLaunchTour() {
    this.props.dispatch(ac.SendToMain({ type: at.MIGRATION_START }));
    this.props.dispatch(ac.UserEvent({ event: at.MIGRATION_START }));
  }

  onCancelTour() {
    this.props.dispatch(ac.SendToMain({ type: at.MIGRATION_CANCEL }));
    this.props.dispatch(ac.UserEvent({ event: at.MIGRATION_CANCEL }));
  }

  render() {
    return React.createElement(
      "div",
      { className: "manual-migration-container" },
      React.createElement(
        "p",
        null,
        React.createElement("span", { className: "icon icon-info" }),
        React.createElement(FormattedMessage, { id: "manual_migration_explanation" })
      ),
      React.createElement(
        "div",
        { className: "manual-migration-actions actions" },
        React.createElement(
          "button",
          { onClick: this.onCancelTour },
          React.createElement(FormattedMessage, { id: "manual_migration_cancel_button" })
        ),
        React.createElement(
          "button",
          { className: "done", onClick: this.onLaunchTour },
          React.createElement(FormattedMessage, { id: "manual_migration_import_button" })
        )
      )
    );
  }
}

module.exports = connect()(ManualMigration);
module.exports._unconnected = ManualMigration;

/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const injectIntl = _require2.injectIntl,
      FormattedMessage = _require2.FormattedMessage;

var _require3 = __webpack_require__(1);

const ac = _require3.actionCreators;


const PreferencesInput = props => React.createElement(
  "section",
  null,
  React.createElement("input", { type: "checkbox", id: props.prefName, name: props.prefName, checked: props.value, onChange: props.onChange, className: props.className }),
  React.createElement(
    "label",
    { htmlFor: props.prefName },
    React.createElement(FormattedMessage, { id: props.titleStringId, values: props.titleStringValues })
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

    // TODO This is temporary until sections register their PreferenceInput component automatically
    try {
      this.topStoriesOptions = JSON.parse(props.Prefs.values["feeds.section.topstories.options"]);
    } catch (e) {
      console.error("Problem parsing feeds.section.topstories.options", e); // eslint-disable-line no-console
    }
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
              titleStringId: "settings_pane_topsites_header", descStringId: "settings_pane_topsites_body" }),
            this.topStoriesOptions && React.createElement(PreferencesInput, { className: "showTopStories", prefName: "feeds.section.topstories",
              value: prefs["feeds.section.topstories"], onChange: this.handleChange,
              titleStringId: "header_recommended_by", titleStringValues: { provider: this.topStoriesOptions.provider_name },
              descStringId: this.topStoriesOptions.provider_description })
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
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals ContentSearchUIController */


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const FormattedMessage = _require2.FormattedMessage,
      injectIntl = _require2.injectIntl;

var _require3 = __webpack_require__(1);

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
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const FormattedMessage = _require2.FormattedMessage;

const Card = __webpack_require__(13);
const Topics = __webpack_require__(22);

class Section extends React.Component {
  render() {
    var _props = this.props;
    const id = _props.id,
          eventSource = _props.eventSource,
          title = _props.title,
          icon = _props.icon,
          rows = _props.rows,
          infoOption = _props.infoOption,
          emptyState = _props.emptyState,
          dispatch = _props.dispatch,
          maxCards = _props.maxCards,
          contextMenuOptions = _props.contextMenuOptions;

    const initialized = rows && rows.length > 0;
    const shouldShowTopics = id === "TopStories" && this.props.topics && this.props.read_more_endpoint;
    // <Section> <-- React component
    // <section> <-- HTML5 element
    return React.createElement(
      "section",
      null,
      React.createElement(
        "div",
        { className: "section-top-bar" },
        React.createElement(
          "h3",
          { className: "section-title" },
          React.createElement("span", { className: `icon icon-small-spacer icon-${icon}` }),
          React.createElement(FormattedMessage, title)
        ),
        infoOption && React.createElement(
          "span",
          { className: "section-info-option" },
          React.createElement(
            "span",
            { className: "sr-only" },
            React.createElement(FormattedMessage, { id: "section_info_option" })
          ),
          React.createElement("img", { className: "info-option-icon" }),
          React.createElement(
            "div",
            { className: "info-option" },
            infoOption.header && React.createElement(
              "div",
              { className: "info-option-header" },
              React.createElement(FormattedMessage, infoOption.header)
            ),
            infoOption.body && React.createElement(
              "p",
              { className: "info-option-body" },
              React.createElement(FormattedMessage, infoOption.body)
            ),
            infoOption.link && React.createElement(
              "a",
              { href: infoOption.link.href, target: "_blank", rel: "noopener noreferrer", className: "info-option-link" },
              React.createElement(FormattedMessage, infoOption.link)
            )
          )
        )
      ),
      React.createElement(
        "ul",
        { className: "section-list", style: { padding: 0 } },
        rows.slice(0, maxCards).map((link, index) => link && React.createElement(Card, { index: index, dispatch: dispatch, link: link, contextMenuOptions: contextMenuOptions, eventSource: eventSource }))
      ),
      !initialized && React.createElement(
        "div",
        { className: "section-empty-state" },
        React.createElement(
          "div",
          { className: "empty-state" },
          React.createElement("img", { className: `empty-state-icon icon icon-${emptyState.icon}` }),
          React.createElement(
            "p",
            { className: "empty-state-message" },
            React.createElement(FormattedMessage, emptyState.message)
          )
        )
      ),
      shouldShowTopics && React.createElement(Topics, { topics: this.props.topics, read_more_endpoint: this.props.read_more_endpoint })
    );
  }
}

class Sections extends React.Component {
  render() {
    const sections = this.props.Sections;
    return React.createElement(
      "div",
      { className: "sections-list" },
      sections.map(section => React.createElement(Section, _extends({ key: section.id }, section, { dispatch: this.props.dispatch })))
    );
  }
}

module.exports = connect(state => ({ Sections: state.Sections }))(Sections);
module.exports._unconnected = Sections;
module.exports.Section = Section;

/***/ }),
/* 21 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const FormattedMessage = _require2.FormattedMessage;

const shortURL = __webpack_require__(4);
const LinkMenu = __webpack_require__(5);

var _require3 = __webpack_require__(1);

const ac = _require3.actionCreators,
      at = _require3.actionTypes;

var _require4 = __webpack_require__(6);

const perfSvc = _require4.perfService;

const TOP_SITES_SOURCE = "TOP_SITES";
const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];

class TopSite extends React.Component {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false, activeTile: null };
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
  }
  toggleContextMenu(event, index) {
    this.setState({
      activeTile: index,
      showContextMenu: true
    });
  }
  onLinkClick() {
    this.props.dispatch(ac.UserEvent({
      event: "CLICK",
      source: TOP_SITES_SOURCE,
      action_position: this.props.index
    }));
  }
  onMenuButtonClick(event) {
    event.preventDefault();
    this.toggleContextMenu(event, this.props.index);
  }
  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
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
      { className: topSiteOuterClassName, key: link.guid || link.url },
      React.createElement(
        "a",
        { href: link.url, onClick: this.onLinkClick },
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
            { dir: "auto" },
            title
          )
        )
      ),
      React.createElement(
        "button",
        { className: "context-menu-button", onClick: this.onMenuButtonClick },
        React.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${title}`
        )
      ),
      React.createElement(LinkMenu, {
        dispatch: dispatch,
        index: index,
        onUpdate: this.onMenuUpdate,
        options: TOP_SITES_CONTEXT_MENU_OPTIONS,
        site: link,
        source: TOP_SITES_SOURCE,
        visible: isContextMenuOpen })
    );
  }
}

/**
 * A proxy class that uses double requestAnimationFrame from
 * componentDidMount to dispatch a SAVE_SESSION_PERF_DATA to the main procsess
 * after the paint.
 *
 * This uses two callbacks because, after one callback, this part of the tree
 * may have rendered but not yet reflowed.  This strategy is modeled after
 * https://stackoverflow.com/a/34999925 but uses a double rFA because
 * we want to get to the closest reliable paint for measuring, and
 * setTimeout is often throttled or queued by browsers in ways that could
 * make it lag too long.
 *
 * XXX Should be made more generic by using this.props.children, or potentially
 * even split out into a higher-order component to wrap whatever.
 *
 * @class TopSitesPerfTimer
 * @extends {React.Component}
 */
class TopSitesPerfTimer extends React.Component {
  constructor(props) {
    super(props);
    // Just for test dependency injection:
    this.perfSvc = this.props.perfSvc || perfSvc;

    this._sendPaintedEvent = this._sendPaintedEvent.bind(this);
    this._timestampSent = false;
  }

  componentDidMount() {
    this._maybeSendPaintedEvent();
  }

  componentDidUpdate() {
    this._maybeSendPaintedEvent();
  }

  /**
   * Call the given callback when the subsequent animation frame
   * (not the upcoming one) paints.
   *
   * @param {Function} callback
   *
   * @returns void
   */
  _onNextFrame(callback) {
    requestAnimationFrame(() => {
      requestAnimationFrame(callback);
    });
  }

  _maybeSendPaintedEvent() {
    // If we've already saved a timestamp for this session, don't do so again.
    if (this._timestampSent) {
      return;
    }

    // We don't want this to ever happen, but sometimes it does.  And when it
    // does (typically on the first newtab at startup time calling
    // componentDidMount), the paint(s) we care about will be later (eg
    // in a subsequent componentDidUpdate).
    if (!this.props.TopSites.initialized) {
      // XXX should send bad event
      return;
    }

    this._onNextFrame(this._sendPaintedEvent);
  }

  _sendPaintedEvent() {
    this.perfSvc.mark("topsites_first_painted_ts");

    try {
      let topsites_first_painted_ts = this.perfSvc.getMostRecentAbsMarkStartByName("topsites_first_painted_ts");

      this.props.dispatch(ac.SendToMain({
        type: at.SAVE_SESSION_PERF_DATA,
        data: { topsites_first_painted_ts }
      }));
    } catch (ex) {
      // If this failed, it's likely because the `privacy.resistFingerprinting`
      // pref is true.  We should at least not blow up, and should continue
      // to set this._timestampSent to avoid going through this again.
    }

    this._timestampSent = true;
  }
  render() {
    return React.createElement(TopSites, this.props);
  }
}

const TopSites = props => React.createElement(
  "section",
  null,
  React.createElement(
    "h3",
    { className: "section-title" },
    React.createElement("span", { className: `icon icon-small-spacer icon-topsites` }),
    React.createElement(FormattedMessage, { id: "header_top_sites" })
  ),
  React.createElement(
    "ul",
    { className: "top-sites-list" },
    props.TopSites.rows.map((link, index) => link && React.createElement(TopSite, {
      key: link.guid || link.url,
      dispatch: props.dispatch,
      link: link,
      index: index }))
  )
);

module.exports = connect(state => ({ TopSites: state.TopSites }))(TopSitesPerfTimer);
module.exports._unconnected = TopSitesPerfTimer;
module.exports.TopSite = TopSite;
module.exports.TopSites = TopSites;

/***/ }),
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(2);

const FormattedMessage = _require.FormattedMessage;


class Topic extends React.Component {
  render() {
    var _props = this.props;
    const url = _props.url,
          name = _props.name;

    return React.createElement(
      "li",
      null,
      React.createElement(
        "a",
        { key: name, className: "topic-link", href: url },
        name
      )
    );
  }
}

class Topics extends React.Component {
  render() {
    var _props2 = this.props;
    const topics = _props2.topics,
          read_more_endpoint = _props2.read_more_endpoint;

    return React.createElement(
      "div",
      { className: "topic" },
      React.createElement(
        "span",
        null,
        React.createElement(FormattedMessage, { id: "pocket_read_more" })
      ),
      React.createElement(
        "ul",
        null,
        topics.map(t => React.createElement(Topic, { key: t.name, url: t.url, name: t.name }))
      ),
      React.createElement(
        "a",
        { className: "topic-read-more", href: read_more_endpoint },
        React.createElement(FormattedMessage, { id: "pocket_read_even_more" }),
        React.createElement("span", { className: "topic-read-more-logo" })
      )
    );
  }
}

module.exports = Topics;
module.exports._unconnected = Topics;
module.exports.Topic = Topic;

/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _require = __webpack_require__(1);

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
      data: { url: site.url, referrer: site.referrer }
    }),
    userEvent: "OPEN_NEW_WINDOW"
  }),
  OpenInPrivateWindow: site => ({
    id: "menu_action_open_private_window",
    icon: "new-window-private",
    action: ac.SendToMain({
      type: at.OPEN_PRIVATE_WINDOW,
      data: { url: site.url, referrer: site.referrer }
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
/* 24 */
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
/* 25 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);
const ReactDOM = __webpack_require__(12);
const Base = __webpack_require__(7);

var _require = __webpack_require__(3);

const Provider = _require.Provider;

const initStore = __webpack_require__(9);

var _require2 = __webpack_require__(11);

const reducers = _require2.reducers;

const DetectUserSessionStart = __webpack_require__(8);

var _require3 = __webpack_require__(10);

const SnippetsProvider = _require3.SnippetsProvider;


new DetectUserSessionStart().sendEventOrAddListener();

const store = initStore(reducers);

ReactDOM.render(React.createElement(
  Provider,
  { store: store },
  React.createElement(Base, null)
), document.getElementById("root"));

// Trigger snippets when snippets data has been received.
const snippets = new SnippetsProvider();
const unsubscribe = store.subscribe(() => {
  const state = store.getState();
  if (state.Snippets.initialized) {
    snippets.init({ appData: state.Snippets });
    unsubscribe();
  }
});

/***/ })
/******/ ]);