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
/******/ 	return __webpack_require__(__webpack_require__.s = 15);
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


const actionTypes = ["BLOCK_URL", "BOOKMARK_URL", "DELETE_BOOKMARK_BY_ID", "DELETE_HISTORY_URL", "INIT", "LOCALE_UPDATED", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_UNLOAD", "NEW_TAB_VISIBLE", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_CHANGED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINK_BLOCKED", "PLACES_LINK_DELETED", "SCREENSHOT_UPDATED", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "TOP_SITES_UPDATED", "UNINIT"
// The line below creates an object like this:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
// It prevents accidentally adding a different key/value name.
].reduce((obj, type) => {
  obj[type] = type;return obj;
}, {});

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

var actionCreators = {
  BroadcastToContent,
  UserEvent,
  UndesiredEvent,
  PerfEvent,
  SendToContent,
  SendToMain
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

module.exports = ReactRedux;

/***/ }),
/* 3 */
/***/ (function(module, exports) {

module.exports = ReactIntl;

/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const addLocaleData = _require2.addLocaleData,
      IntlProvider = _require2.IntlProvider;

const TopSites = __webpack_require__(12);
const Search = __webpack_require__(11);

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
    var _props$App = this.props.App;
    let locale = _props$App.locale,
        strings = _props$App.strings,
        initialized = _props$App.initialized;

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
          React.createElement(Search, null),
          React.createElement(TopSites, null)
        )
      )
    );
  }
}

module.exports = connect(state => ({ App: state.App }))(Base);

/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _require = __webpack_require__(1);

const at = _require.actionTypes;


const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

module.exports = class DetectUserSessionStart {
  constructor() {
    let options = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {};

    // Overrides for testing
    this.sendAsyncMessage = options.sendAsyncMessage || window.sendAsyncMessage;
    this.document = options.document || document;

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
   * _sendEvent - Sends a message to the main process to indicate the current tab
   *             is now visible to the user.
   */
  _sendEvent() {
    this.sendAsyncMessage("ActivityStream:ContentToMain", { type: at.NEW_TAB_VISIBLE });
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
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* eslint-env mozilla/frame-script */

var _require = __webpack_require__(14);

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
    store.dispatch(msg.data);
  });

  return store;
};

module.exports.MERGE_STORE_ACTION = MERGE_STORE_ACTION;
module.exports.OUTGOING_MESSAGE_NAME = OUTGOING_MESSAGE_NAME;
module.exports.INCOMING_MESSAGE_NAME = INCOMING_MESSAGE_NAME;

/***/ }),
/* 7 */
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
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: []
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

function TopSites() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.TopSites;
  let action = arguments[1];

  let hasMatch;
  let newRows;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, { initialized: true, rows: action.data });
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, { rows: newRows }) : prevState;
    case at.PLACES_BOOKMARK_ADDED:
      newRows = prevState.rows.map(site => {
        if (site.url === action.data.url) {
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
        if (site.url === action.data.url) {
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
      newRows = prevState.rows.filter(val => val.url !== action.data.url);
      return Object.assign({}, prevState, { rows: newRows });
    default:
      return prevState;
  }
}

var reducers = { TopSites, App };
module.exports = {
  reducers,
  INITIAL_STATE
};

/***/ }),
/* 8 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 9 */
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
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(3);

const injectIntl = _require.injectIntl;

const ContextMenu = __webpack_require__(9);

var _require2 = __webpack_require__(1);

const actionTypes = _require2.actionTypes,
      ac = _require2.actionCreators;


class LinkMenu extends React.Component {
  getBookmarkStatus(site) {
    return site.bookmarkGuid ? {
      id: "menu_action_remove_bookmark",
      icon: "bookmark-remove",
      action: "DELETE_BOOKMARK_BY_ID",
      data: site.bookmarkGuid,
      userEvent: "BOOKMARK_DELETE"
    } : {
      id: "menu_action_bookmark",
      icon: "bookmark",
      action: "BOOKMARK_URL",
      data: site.url,
      userEvent: "BOOKMARK_ADD"
    };
  }
  getDefaultContextMenu(site) {
    return [{
      id: "menu_action_open_new_window",
      icon: "new-window",
      action: "OPEN_NEW_WINDOW",
      data: { url: site.url },
      userEvent: "OPEN_NEW_WINDOW"
    }, {
      id: "menu_action_open_private_window",
      icon: "new-window-private",
      action: "OPEN_PRIVATE_WINDOW",
      data: { url: site.url },
      userEvent: "OPEN_PRIVATE_WINDOW"
    }];
  }
  getOptions() {
    var _props = this.props;
    const dispatch = _props.dispatch,
          site = _props.site,
          index = _props.index,
          source = _props.source;

    // default top sites have a limited set of context menu options

    let options = this.getDefaultContextMenu(site);

    // all other top sites have all the following context menu options
    if (!site.isDefault) {
      options = [this.getBookmarkStatus(site), { type: "separator" }, ...options, { type: "separator" }, {
        id: "menu_action_dismiss",
        icon: "dismiss",
        action: "BLOCK_URL",
        data: site.url,
        userEvent: "BLOCK"
      }, {
        id: "menu_action_delete",
        icon: "delete",
        action: "DELETE_HISTORY_URL",
        data: site.url,
        userEvent: "DELETE"
      }];
    }
    options.forEach(option => {
      const action = option.action,
            data = option.data,
            id = option.id,
            type = option.type,
            userEvent = option.userEvent;
      // Convert message ids to localized labels and add onClick function

      if (!type && id) {
        option.label = this.props.intl.formatMessage(option);
        option.onClick = () => {
          dispatch(ac.SendToMain({ type: actionTypes[action], data }));
          dispatch(ac.UserEvent({
            event: userEvent,
            source,
            action_position: index
          }));
        };
      }
    });

    // this is for a11y - we want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly
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
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals ContentSearchUIController */


const React = __webpack_require__(0);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

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
      this.controller = new ContentSearchUIController(input, input.parentNode, "newtab", "activity");
      addEventListener("ContentSearchClient", this);
    } else {
      this.controller = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  render() {
    return React.createElement(
      "form",
      { className: "search-wrapper" },
      React.createElement(
        "label",
        { htmlFor: "search-input", className: "search-label" },
        React.createElement(
          "span",
          { className: "sr-only" },
          React.createElement(FormattedMessage, { id: "search_web_placeholder" })
        )
      ),
      React.createElement("input", {
        id: "search-input",
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
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(2);

const connect = _require.connect;

var _require2 = __webpack_require__(3);

const FormattedMessage = _require2.FormattedMessage;

const shortURL = __webpack_require__(13);
const LinkMenu = __webpack_require__(10);

var _require3 = __webpack_require__(1);

const ac = _require3.actionCreators;

const TOP_SITES_SOURCE = "TOP_SITES";

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
    const title = shortURL(link);
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
          { className: "title" },
          title
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
        source: TOP_SITES_SOURCE })
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
    props.TopSites.rows.map((link, index) => React.createElement(TopSite, {
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
/* 13 */
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
  return hostname.slice(0, eTLDExtra).toLowerCase();
};

/***/ }),
/* 14 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);
const ReactDOM = __webpack_require__(8);
const Base = __webpack_require__(4);

var _require = __webpack_require__(2);

const Provider = _require.Provider;

const initStore = __webpack_require__(6);

var _require2 = __webpack_require__(7);

const reducers = _require2.reducers;

const DetectUserSessionStart = __webpack_require__(5);

new DetectUserSessionStart().sendEventOrAddListener();

const store = initStore(reducers);

ReactDOM.render(React.createElement(
  Provider,
  { store: store },
  React.createElement(Base, null)
), document.getElementById("root"));

/***/ })
/******/ ]);