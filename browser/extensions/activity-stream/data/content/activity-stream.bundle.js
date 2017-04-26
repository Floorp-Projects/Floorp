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
/******/ 	return __webpack_require__(__webpack_require__.s = 10);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports) {

module.exports = React;

/***/ }),
/* 1 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 2 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const MAIN_MESSAGE_TYPE = "ActivityStream:Main";
const CONTENT_MESSAGE_TYPE = "ActivityStream:Content";

const actionTypes = ["INIT", "UNINIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_UNLOAD", "PERFORM_SEARCH", "SCREENSHOT_UPDATED", "SEARCH_STATE_UPDATED", "TOP_SITES_UPDATED"
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
 * @param  {string} options.fromTarget The id of the content port from which the action originated. (optional)
 * @return {object} An action with added .meta properties
 */
function SendToMain(action) {
  let options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};

  return _RouteMessage(action, {
    from: CONTENT_MESSAGE_TYPE,
    to: MAIN_MESSAGE_TYPE,
    fromTarget: options.fromTarget
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

var actionCreators = {
  SendToMain,
  SendToContent,
  BroadcastToContent
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
  _RouteMessage
};
module.exports = {
  actionTypes,
  actionCreators,
  actionUtils,
  MAIN_MESSAGE_TYPE,
  CONTENT_MESSAGE_TYPE
};

/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);
const TopSites = __webpack_require__(8);
const Search = __webpack_require__(7);

const Base = () => React.createElement(
  "div",
  { className: "outer-wrapper" },
  React.createElement(
    "main",
    null,
    React.createElement(Search, null),
    React.createElement(TopSites, null)
  )
);

module.exports = Base;

/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* globals sendAsyncMessage, addMessageListener */

var _require = __webpack_require__(9);

const createStore = _require.createStore,
      combineReducers = _require.combineReducers,
      applyMiddleware = _require.applyMiddleware;

var _require2 = __webpack_require__(2);

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
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var _require = __webpack_require__(2);

const at = _require.actionTypes;


const INITIAL_STATE = {
  TopSites: {
    init: false,
    rows: []
  },
  Search: {
    currentEngine: {
      name: "",
      icon: ""
    },
    engines: []
  }
};

// TODO: Handle some real actions here, once we have a TopSites feed working
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
      return Object.assign({}, prevState, { init: true, rows: action.data });
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, { rows: newRows }) : prevState;
    default:
      return prevState;
  }
}

function Search() {
  let prevState = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : INITIAL_STATE.Search;
  let action = arguments[1];

  switch (action.type) {
    case at.SEARCH_STATE_UPDATED:
      {
        if (!action.data) {
          return prevState;
        }
        var _action$data = action.data;
        let currentEngine = _action$data.currentEngine,
            engines = _action$data.engines;

        return Object.assign({}, prevState, {
          currentEngine,
          engines
        });
      }
    default:
      return prevState;
  }
}
var reducers = { TopSites, Search };
module.exports = {
  reducers,
  INITIAL_STATE
};

/***/ }),
/* 6 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(1);

const connect = _require.connect;

var _require2 = __webpack_require__(2);

const actionTypes = _require2.actionTypes,
      actionCreators = _require2.actionCreators;


const Search = React.createClass({
  displayName: "Search",

  getInitialState() {
    return { searchString: "" };
  },
  performSearch(options) {
    let searchData = {
      engineName: options.engineName,
      searchString: options.searchString,
      searchPurpose: "newtab",
      healthReportKey: "newtab"
    };
    this.props.dispatch(actionCreators.SendToMain({ type: actionTypes.PERFORM_SEARCH, data: searchData }));
  },
  onClick(event) {
    const currentEngine = this.props.Search.currentEngine;

    event.preventDefault();
    this.performSearch({ engineName: currentEngine.name, searchString: this.state.searchString });
  },
  onChange(event) {
    this.setState({ searchString: event.target.value });
  },
  render() {
    return React.createElement(
      "form",
      { className: "search-wrapper" },
      React.createElement("span", { className: "search-label" }),
      React.createElement("input", { value: this.state.searchString, type: "search",
        onChange: this.onChange,
        maxLength: "256", title: "Submit search",
        placeholder: "Search the Web" }),
      React.createElement("button", { onClick: this.onClick })
    );
  }
});

module.exports = connect(state => ({ Search: state.Search }))(Search);

/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const React = __webpack_require__(0);

var _require = __webpack_require__(1);

const connect = _require.connect;


function displayURL(url) {
  return new URL(url).hostname.replace(/^www./, "");
}

const TopSites = props => React.createElement(
  "section",
  null,
  React.createElement(
    "h3",
    { className: "section-title" },
    "Top Sites"
  ),
  React.createElement(
    "ul",
    { className: "top-sites-list" },
    props.TopSites.rows.map(link => {
      const title = displayURL(link.url);
      const className = `screenshot${link.screenshot ? " active" : ""}`;
      const style = { backgroundImage: link.screenshot ? `url(${link.screenshot})` : "none" };
      return React.createElement(
        "li",
        { key: link.url },
        React.createElement(
          "a",
          { href: link.url },
          React.createElement(
            "div",
            { className: "tile" },
            React.createElement(
              "span",
              { className: "letter-fallback", ariaHidden: true },
              title[0]
            ),
            React.createElement("div", { className: className, style: style })
          ),
          React.createElement(
            "div",
            { className: "title" },
            title
          )
        )
      );
    })
  )
);

module.exports = connect(state => ({ TopSites: state.TopSites }))(TopSites);

/***/ }),
/* 9 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* globals addMessageListener, removeMessageListener */
const React = __webpack_require__(0);
const ReactDOM = __webpack_require__(6);
const Base = __webpack_require__(3);

var _require = __webpack_require__(1);

const Provider = _require.Provider;

const initStore = __webpack_require__(4);

var _require2 = __webpack_require__(5);

const reducers = _require2.reducers;


const store = initStore(reducers);

ReactDOM.render(React.createElement(
  Provider,
  { store: store },
  React.createElement(Base, null)
), document.getElementById("root"));

/***/ })
/******/ ]);