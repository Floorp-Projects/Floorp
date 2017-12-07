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
/******/ 	return __webpack_require__(__webpack_require__.s = 12);
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
for (const type of ["BLOCK_URL", "BOOKMARK_URL", "DELETE_BOOKMARK_BY_ID", "DELETE_HISTORY_URL", "DELETE_HISTORY_URL_CONFIRM", "DIALOG_CANCEL", "DIALOG_OPEN", "DISABLE_ONBOARDING", "INIT", "MIGRATION_CANCEL", "MIGRATION_COMPLETED", "MIGRATION_START", "NEW_TAB_INIT", "NEW_TAB_INITIAL_STATE", "NEW_TAB_LOAD", "NEW_TAB_REHYDRATED", "NEW_TAB_STATE_REQUEST", "NEW_TAB_UNLOAD", "OPEN_LINK", "OPEN_NEW_WINDOW", "OPEN_PRIVATE_WINDOW", "PAGE_PRERENDERED", "PLACES_BOOKMARK_ADDED", "PLACES_BOOKMARK_CHANGED", "PLACES_BOOKMARK_REMOVED", "PLACES_HISTORY_CLEARED", "PLACES_LINKS_DELETED", "PLACES_LINK_BLOCKED", "PREFS_INITIAL_VALUES", "PREF_CHANGED", "RICH_ICON_MISSING", "SAVE_SESSION_PERF_DATA", "SAVE_TO_POCKET", "SCREENSHOT_UPDATED", "SECTION_DEREGISTER", "SECTION_DISABLE", "SECTION_ENABLE", "SECTION_OPTIONS_CHANGED", "SECTION_REGISTER", "SECTION_UPDATE", "SECTION_UPDATE_CARD", "SETTINGS_CLOSE", "SETTINGS_OPEN", "SET_PREF", "SHOW_FIREFOX_ACCOUNTS", "SNIPPETS_BLOCKLIST_UPDATED", "SNIPPETS_DATA", "SNIPPETS_RESET", "SNIPPET_BLOCKED", "SYSTEM_TICK", "TELEMETRY_IMPRESSION_STATS", "TELEMETRY_PERFORMANCE_EVENT", "TELEMETRY_UNDESIRED_EVENT", "TELEMETRY_USER_EVENT", "TOP_SITES_ADD", "TOP_SITES_CANCEL_EDIT", "TOP_SITES_EDIT", "TOP_SITES_PIN", "TOP_SITES_UNPIN", "TOP_SITES_UPDATED", "UNINIT"]) {
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
 * @param  {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a SendToMain action.
 */
function UndesiredEvent(data, importContext = globalImportContext) {
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
 * @param  {int} importContext (For testing) Override the import context for testing.
 * @return {object} An action. For UI code, a SendToMain action.
 */
function PerfEvent(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.TELEMETRY_PERFORMANCE_EVENT,
    data
  };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

/**
 * ImpressionStats - A telemetry ping indicating an impression stats.
 *
 * @param  {object} data Fields to include in the ping
 * @param  {int} importContext (For testing) Override the import context for testing.
 * #return {object} An action. For UI code, a SendToMain action.
 */
function ImpressionStats(data, importContext = globalImportContext) {
  const action = {
    type: actionTypes.TELEMETRY_IMPRESSION_STATS,
    data
  };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

function SetPref(name, value, importContext = globalImportContext) {
  const action = { type: actionTypes.SET_PREF, data: { name, value } };
  return importContext === UI_CODE ? SendToMain(action) : action;
}

var actionCreators = {
  BroadcastToContent,
  UserEvent,
  UndesiredEvent,
  PerfEvent,
  ImpressionStats,
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

module.exports = ReactIntl;

/***/ }),
/* 3 */
/***/ (function(module, exports) {

module.exports = ReactRedux;

/***/ }),
/* 4 */
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
/* 5 */
/***/ (function(module, exports) {

module.exports = {
  TOP_SITES_SOURCE: "TOP_SITES",
  TOP_SITES_CONTEXT_MENU_OPTIONS: ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"],
  // minimum size necessary to show a rich icon instead of a screenshot
  MIN_RICH_FAVICON_SIZE: 96,
  // minimum size necessary to show any icon in the top left corner with a screenshot
  MIN_CORNER_FAVICON_SIZE: 16
};

/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const { actionTypes: at } = __webpack_require__(0);
const { Dedupe } = __webpack_require__(20);

const TOP_SITES_DEFAULT_LENGTH = 6;
const TOP_SITES_SHOWMORE_LENGTH = 12;

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
    // Used in content only to dispatch action from
    // context menu to TopSitesEdit.
    editForm: {
      visible: false,
      site: null
    }
  },
  Prefs: {
    initialized: false,
    values: {}
  },
  Dialog: {
    visible: false,
    data: {}
  },
  Sections: [],
  PreferencesPane: { visible: false }
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case at.INIT:
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
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, { initialized: true, rows: action.data });
    case at.TOP_SITES_EDIT:
      return Object.assign({}, prevState, { editForm: { visible: true, site: action.data } });
    case at.TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, { editForm: { visible: false } });
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
          const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
          return Object.assign({}, site, { bookmarkGuid, bookmarkTitle, bookmarkDateCreated: dateAdded });
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
    case at.BLOCK_URL:
    case at.DELETE_HISTORY_URL:
      // Optimistically update the UI by responding to the context menu action
      // events and removing the site that was blocked/deleted with an empty slot.
      // Once refresh() finishes, we update the UI again with a new site
      newRows = prevState.rows.filter(val => val && val.url !== action.data.url);
      return Object.assign({}, prevState, { rows: newRows });
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
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

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
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

function Sections(prevState = INITIAL_STATE.Sections, action) {
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

      // Invariant: Sections array sorted in increasing order of property `order`.
      // If section doesn't exist in prevState, create a new section object. If
      // the section has an order, insert it at the correct place in the array.
      // Otherwise, prepend it and set the order to be minimal.
      if (!hasMatch) {
        const initialized = !!(action.data.rows && action.data.rows.length > 0);
        let order;
        let index;
        if (prevState.length > 0) {
          order = action.data.order !== undefined ? action.data.order : prevState[0].order - 1;
          index = newState.findIndex(section => section.order >= order);
          if (index === -1) {
            index = newState.length;
          }
        } else {
          order = action.data.order !== undefined ? action.data.order : 0;
          index = 0;
        }

        const section = Object.assign({ title: "", rows: [], order, enabled: false }, action.data, { initialized });
        newState.splice(index, 0, section);
      }
      return newState;
    case at.SECTION_UPDATE:
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          // If the action is updating rows, we should consider initialized to be true.
          // This can be overridden if initialized is defined in the action.data
          const initialized = action.data.rows ? { initialized: true } : {};
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
    case at.SECTION_UPDATE_CARD:
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
    case at.PLACES_BOOKMARK_ADDED:
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
            if (!newSite.type || newSite.type === "bookmark") {
              newSite.type = "history";
            }
            return newSite;
          }
          return item;
        })
      }));
    case at.PLACES_LINKS_DELETED:
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => !action.data.includes(site.url)) }));
    case at.PLACES_LINK_BLOCKED:
      return prevState.map(section => Object.assign({}, section, { rows: section.rows.filter(site => site.url !== action.data.url) }));
    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case at.SNIPPETS_DATA:
      return Object.assign({}, prevState, { initialized: true }, action.data);
    case at.SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

function PreferencesPane(prevState = INITIAL_STATE.PreferencesPane, action) {
  switch (action.type) {
    case at.SETTINGS_OPEN:
      return Object.assign({}, prevState, { visible: true });
    case at.SETTINGS_CLOSE:
      return Object.assign({}, prevState, { visible: false });
    default:
      return prevState;
  }
}

var reducers = { TopSites, App, Snippets, Prefs, Dialog, Sections, PreferencesPane };
module.exports = {
  reducers,
  INITIAL_STATE,
  insertPinned,
  TOP_SITES_DEFAULT_LENGTH,
  TOP_SITES_SHOWMORE_LENGTH
};

/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

const React = __webpack_require__(1);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);

const LinkMenu = __webpack_require__(8);

const { TOP_SITES_SOURCE, TOP_SITES_CONTEXT_MENU_OPTIONS, MIN_RICH_FAVICON_SIZE, MIN_CORNER_FAVICON_SIZE } = __webpack_require__(5);

const TopSiteLink = props => {
  const { link, title } = props;
  const topSiteOuterClassName = `top-site-outer${props.className ? ` ${props.className}` : ""}`;
  const { tippyTopIcon, faviconSize } = link;
  const letterFallback = title[0];
  let imageClassName;
  let imageStyle;
  let showSmallFavicon = false;
  let smallFaviconStyle;
  let smallFaviconFallback;
  if (tippyTopIcon || faviconSize >= MIN_RICH_FAVICON_SIZE) {
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
    if (faviconSize >= MIN_CORNER_FAVICON_SIZE) {
      showSmallFavicon = true;
      smallFaviconStyle = { backgroundImage: `url(${link.favicon})` };
    } else if (link.screenshot) {
      // Don't show a small favicon if there is no screenshot, because that
      // would result in two fallback icons
      showSmallFavicon = true;
      smallFaviconFallback = true;
    }
  }
  return React.createElement(
    "li",
    { className: topSiteOuterClassName, key: link.guid || link.url },
    React.createElement(
      "a",
      { href: link.url, onClick: props.onClick },
      React.createElement(
        "div",
        { className: "tile", "aria-hidden": true, "data-fallback": letterFallback },
        React.createElement("div", { className: imageClassName, style: imageStyle }),
        showSmallFavicon && React.createElement("div", {
          className: "top-site-icon default-icon",
          "data-fallback": smallFaviconFallback && letterFallback,
          style: smallFaviconStyle })
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
    props.children
  );
};
TopSiteLink.defaultProps = {
  title: "",
  link: {}
};

class TopSite extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false, activeTile: null };
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onDismissButtonClick = this.onDismissButtonClick.bind(this);
    this.onPinButtonClick = this.onPinButtonClick.bind(this);
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
  }
  toggleContextMenu(event, index) {
    this.setState({
      activeTile: index,
      showContextMenu: true
    });
  }
  userEvent(event) {
    this.props.dispatch(ac.UserEvent({
      event,
      source: TOP_SITES_SOURCE,
      action_position: this.props.index
    }));
  }
  onLinkClick(ev) {
    if (this.props.onEdit) {
      // Ignore clicks if we are in the edit modal.
      ev.preventDefault();
      return;
    }
    this.userEvent("CLICK");
  }
  onMenuButtonClick(event) {
    event.preventDefault();
    this.toggleContextMenu(event, this.props.index);
  }
  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }
  onDismissButtonClick() {
    const { link } = this.props;
    if (link.isPinned) {
      this.props.dispatch(ac.SendToMain({
        type: at.TOP_SITES_UNPIN,
        data: { site: { url: link.url } }
      }));
    }
    this.props.dispatch(ac.SendToMain({
      type: at.BLOCK_URL,
      data: link.url
    }));
    this.userEvent("BLOCK");
  }
  onPinButtonClick() {
    const { link, index } = this.props;
    if (link.isPinned) {
      this.props.dispatch(ac.SendToMain({
        type: at.TOP_SITES_UNPIN,
        data: { site: { url: link.url } }
      }));
      this.userEvent("UNPIN");
    } else {
      this.props.dispatch(ac.SendToMain({
        type: at.TOP_SITES_PIN,
        data: { site: { url: link.url }, index }
      }));
      this.userEvent("PIN");
    }
  }
  onEditButtonClick() {
    this.props.onEdit(this.props.index);
  }
  render() {
    const { props } = this;
    const { link } = props;
    const isContextMenuOpen = this.state.showContextMenu && this.state.activeTile === props.index;
    const title = link.label || link.hostname;
    return React.createElement(
      TopSiteLink,
      _extends({}, props, { onClick: this.onLinkClick, className: isContextMenuOpen ? "active" : "", title: title }),
      !props.onEdit && React.createElement(
        "div",
        null,
        React.createElement(
          "button",
          { className: "context-menu-button icon", onClick: this.onMenuButtonClick },
          React.createElement(
            "span",
            { className: "sr-only" },
            `Open context menu for ${title}`
          )
        ),
        React.createElement(LinkMenu, {
          dispatch: props.dispatch,
          index: props.index,
          onUpdate: this.onMenuUpdate,
          options: TOP_SITES_CONTEXT_MENU_OPTIONS,
          site: link,
          source: TOP_SITES_SOURCE,
          visible: isContextMenuOpen })
      ),
      props.onEdit && React.createElement(
        "div",
        { className: "edit-menu" },
        React.createElement("button", {
          className: `icon icon-${link.isPinned ? "unpin" : "pin"}`,
          title: this.props.intl.formatMessage({ id: `edit_topsites_${link.isPinned ? "unpin" : "pin"}_button` }),
          onClick: this.onPinButtonClick }),
        React.createElement("button", {
          className: "icon icon-edit",
          title: this.props.intl.formatMessage({ id: "edit_topsites_edit_button" }),
          onClick: this.onEditButtonClick }),
        React.createElement("button", {
          className: "icon icon-dismiss",
          title: this.props.intl.formatMessage({ id: "edit_topsites_dismiss_button" }),
          onClick: this.onDismissButtonClick })
      )
    );
  }
}
TopSite.defaultProps = { link: {} };

const TopSitePlaceholder = () => React.createElement(TopSiteLink, { className: "placeholder" });

const TopSiteList = props => {
  const topSites = props.TopSites.rows.slice(0, props.TopSitesCount);
  const topSitesUI = [];
  for (let i = 0, l = props.TopSitesCount; i < l; i++) {
    const link = topSites[i];
    topSitesUI.push(!link ? React.createElement(TopSitePlaceholder, { key: i }) : React.createElement(TopSite, {
      key: link.guid || link.url,
      dispatch: props.dispatch,
      link: link,
      index: i,
      intl: props.intl,
      onEdit: props.onEdit }));
  }
  return React.createElement(
    "ul",
    { className: "top-sites-list" },
    topSitesUI
  );
};

module.exports.TopSite = TopSite;
module.exports.TopSiteLink = TopSiteLink;
module.exports.TopSitePlaceholder = TopSitePlaceholder;
module.exports.TopSiteList = TopSiteList;

/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { injectIntl } = __webpack_require__(2);
const ContextMenu = __webpack_require__(18);
const { actionCreators: ac } = __webpack_require__(0);
const linkMenuOptions = __webpack_require__(19);
const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];

class LinkMenu extends React.PureComponent {
  getOptions() {
    const props = this.props;
    const { site, index, source } = props;

    // Handle special case of default site
    const propOptions = !site.isDefault ? props.options : DEFAULT_SITE_MENU_OPTIONS;

    const options = propOptions.map(o => linkMenuOptions[o](site, index, source)).map(option => {
      const { action, impression, id, type, userEvent } = option;
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
    return React.createElement(ContextMenu, {
      visible: this.props.visible,
      onUpdate: this.props.onUpdate,
      options: this.getOptions() });
  }
}

module.exports = injectIntl(LinkMenu);
module.exports._unconnected = LinkMenu;

/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

const React = __webpack_require__(1);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);
const { injectIntl, FormattedMessage } = __webpack_require__(2);

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

function getFormattedMessage(message) {
  return typeof message === "string" ? React.createElement(
    "span",
    null,
    message
  ) : React.createElement(FormattedMessage, message);
}
function getCollapsed(props) {
  return props.Prefs.values[props.prefName];
}

class Info extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onInfoEnter = this.onInfoEnter.bind(this);
    this.onInfoLeave = this.onInfoLeave.bind(this);
    this.onManageClick = this.onManageClick.bind(this);
    this.state = { infoActive: false };
  }

  /**
   * Take a truthy value to conditionally change the infoActive state.
   */
  _setInfoState(nextActive) {
    const infoActive = !!nextActive;
    if (infoActive !== this.state.infoActive) {
      this.setState({ infoActive });
    }
  }
  onInfoEnter() {
    // We're getting focus or hover, so info state should be true if not yet.
    this._setInfoState(true);
  }
  onInfoLeave(event) {
    // We currently have an active (true) info state, so keep it true only if we
    // have a related event target that is contained "within" the current target
    // (section-info-option) as itself or a descendant. Set to false otherwise.
    this._setInfoState(event && event.relatedTarget && (event.relatedTarget === event.currentTarget || event.relatedTarget.compareDocumentPosition(event.currentTarget) & Node.DOCUMENT_POSITION_CONTAINS));
  }
  onManageClick() {
    this.props.dispatch({ type: at.SETTINGS_OPEN });
    this.props.dispatch(ac.UserEvent({ event: "OPEN_NEWTAB_PREFS" }));
  }
  render() {
    const { infoOption, intl } = this.props;
    const infoOptionIconA11yAttrs = {
      "aria-haspopup": "true",
      "aria-controls": "info-option",
      "aria-expanded": this.state.infoActive ? "true" : "false",
      "role": "note",
      "tabIndex": 0
    };
    const sectionInfoTitle = intl.formatMessage({ id: "section_info_option" });

    return React.createElement(
      "span",
      { className: "section-info-option",
        onBlur: this.onInfoLeave,
        onFocus: this.onInfoEnter,
        onMouseOut: this.onInfoLeave,
        onMouseOver: this.onInfoEnter },
      React.createElement("img", _extends({ className: "info-option-icon", title: sectionInfoTitle
      }, infoOptionIconA11yAttrs)),
      React.createElement(
        "div",
        { className: "info-option" },
        infoOption.header && React.createElement(
          "div",
          { className: "info-option-header", role: "heading" },
          getFormattedMessage(infoOption.header)
        ),
        React.createElement(
          "p",
          { className: "info-option-body" },
          infoOption.body && getFormattedMessage(infoOption.body),
          infoOption.link && React.createElement(
            "a",
            { href: infoOption.link.href, target: "_blank", rel: "noopener noreferrer", className: "info-option-link" },
            getFormattedMessage(infoOption.link.title || infoOption.link)
          )
        ),
        React.createElement(
          "div",
          { className: "info-option-manage" },
          React.createElement(
            "button",
            { onClick: this.onManageClick },
            React.createElement(FormattedMessage, { id: "settings_pane_header" })
          )
        )
      )
    );
  }
}

const InfoIntl = injectIntl(Info);

class Disclaimer extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onAcknowledge = this.onAcknowledge.bind(this);
  }

  onAcknowledge() {
    this.props.dispatch(ac.SetPref(this.props.disclaimerPref, false));
    this.props.dispatch(ac.UserEvent({ event: "SECTION_DISCLAIMER_ACKNOWLEDGED", source: this.props.eventSource }));
  }

  render() {
    const disclaimer = this.props.disclaimer;
    return React.createElement(
      "div",
      { className: "section-disclaimer" },
      React.createElement(
        "div",
        { className: "section-disclaimer-text" },
        getFormattedMessage(disclaimer.text),
        disclaimer.link && React.createElement(
          "a",
          { href: disclaimer.link.href, target: "_blank", rel: "noopener noreferrer" },
          getFormattedMessage(disclaimer.link.title || disclaimer.link)
        )
      ),
      React.createElement(
        "button",
        { onClick: this.onAcknowledge },
        getFormattedMessage(disclaimer.button)
      )
    );
  }
}

const DisclaimerIntl = injectIntl(Disclaimer);

class CollapsibleSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.onInfoEnter = this.onInfoEnter.bind(this);
    this.onInfoLeave = this.onInfoLeave.bind(this);
    this.onHeaderClick = this.onHeaderClick.bind(this);
    this.onTransitionEnd = this.onTransitionEnd.bind(this);
    this.enableOrDisableAnimation = this.enableOrDisableAnimation.bind(this);
    this.state = { enableAnimation: true, isAnimating: false, infoActive: false };
  }

  componentWillMount() {
    this.props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this.enableOrDisableAnimation);
  }
  componentWillUpdate(nextProps) {
    // Check if we're about to go from expanded to collapsed
    if (!getCollapsed(this.props) && getCollapsed(nextProps)) {
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
  _setInfoState(nextActive) {
    // Take a truthy value to conditionally change the infoActive state.
    const infoActive = !!nextActive;
    if (infoActive !== this.state.infoActive) {
      this.setState({ infoActive });
    }
  }
  onBodyMount(node) {
    this.sectionBody = node;
  }
  onInfoEnter() {
    // We're getting focus or hover, so info state should be true if not yet.
    this._setInfoState(true);
  }
  onInfoLeave(event) {
    // We currently have an active (true) info state, so keep it true only if we
    // have a related event target that is contained "within" the current target
    // (section-info-option) as itself or a descendant. Set to false otherwise.
    this._setInfoState(event && event.relatedTarget && (event.relatedTarget === event.currentTarget || event.relatedTarget.compareDocumentPosition(event.currentTarget) & Node.DOCUMENT_POSITION_CONTAINS));
  }
  onHeaderClick() {
    // Get the current height of the body so max-height transitions can work
    this.setState({
      isAnimating: true,
      maxHeight: `${this.sectionBody.scrollHeight}px`
    });
    this.props.dispatch(ac.SetPref(this.props.prefName, !getCollapsed(this.props)));
  }
  onTransitionEnd(event) {
    // Only update the animating state for our own transition (not a child's)
    if (event.target === event.currentTarget) {
      this.setState({ isAnimating: false });
    }
  }
  renderIcon() {
    const icon = this.props.icon;
    if (icon && icon.startsWith("moz-extension://")) {
      return React.createElement("span", { className: "icon icon-small-spacer", style: { backgroundImage: `url('${icon}')` } });
    }
    return React.createElement("span", { className: `icon icon-small-spacer icon-${icon || "webextension"}` });
  }
  render() {
    const isCollapsed = getCollapsed(this.props);
    const { enableAnimation, isAnimating, maxHeight } = this.state;
    const { id, infoOption, eventSource, disclaimer } = this.props;
    const disclaimerPref = `section.${id}.showDisclaimer`;
    const needsDisclaimer = disclaimer && this.props.Prefs.values[disclaimerPref];

    return React.createElement(
      "section",
      { className: `collapsible-section ${this.props.className}${enableAnimation ? " animation-enabled" : ""}${isCollapsed ? " collapsed" : ""}` },
      React.createElement(
        "div",
        { className: "section-top-bar" },
        React.createElement(
          "h3",
          { className: "section-title" },
          React.createElement(
            "span",
            { className: "click-target", onClick: this.onHeaderClick },
            this.renderIcon(),
            this.props.title,
            React.createElement("span", { className: `icon ${isCollapsed ? "icon-arrowhead-forward" : "icon-arrowhead-down"}` })
          )
        ),
        infoOption && React.createElement(InfoIntl, { infoOption: infoOption, dispatch: this.props.dispatch })
      ),
      React.createElement(
        "div",
        {
          className: `section-body${isAnimating ? " animating" : ""}`,
          onTransitionEnd: this.onTransitionEnd,
          ref: this.onBodyMount,
          style: isAnimating && !isCollapsed ? { maxHeight } : null },
        needsDisclaimer && React.createElement(DisclaimerIntl, { disclaimerPref: disclaimerPref, disclaimer: disclaimer, eventSource: eventSource, dispatch: this.props.dispatch }),
        this.props.children
      )
    );
  }
}

CollapsibleSection.defaultProps = {
  document: global.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden"
  },
  Prefs: { values: {} }
};

module.exports = injectIntl(CollapsibleSection);
module.exports._unconnected = CollapsibleSection;
module.exports.Info = Info;
module.exports.InfoIntl = InfoIntl;
module.exports.Disclaimer = Disclaimer;
module.exports.DisclaimerIntl = DisclaimerIntl;
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);
const { perfService: perfSvc } = __webpack_require__(11);

// Currently record only a fixed set of sections. This will prevent data
// from custom sections from showing up or from topstories.
const RECORDED_SECTIONS = ["highlights", "topsites"];

class ComponentPerfTimer extends React.Component {
  constructor(props) {
    super(props);
    // Just for test dependency injection:
    this.perfSvc = this.props.perfSvc || perfSvc;

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
      this.props.dispatch(ac.SendToMain({
        type: at.SAVE_SESSION_PERF_DATA,
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

      this.props.dispatch(ac.SendToMain({
        type: at.SAVE_SESSION_PERF_DATA,
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

module.exports = ComponentPerfTimer;

/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals Services */


/* istanbul ignore if */
// Note: normally we would just feature detect Components.utils here, but
// unfortunately that throws an ugly warning in content if we do.

if (typeof Window === "undefined" && typeof Components !== "undefined" && Components.utils) {
  Components.utils.import("resource://gre/modules/Services.jsm");
}

let usablePerfObj;

/* istanbul ignore if */
/* istanbul ignore else */
if (typeof Services !== "undefined") {
  // Borrow the high-resolution timer from the hidden window....
  usablePerfObj = Services.appShell.hiddenDOMWindow.performance;
} else if (typeof performance !== "undefined") {
  // we must be running in content space
  usablePerfObj = performance;
} else {
  // This is a dummy object so this file doesn't crash in the node prerendering
  // task.
  usablePerfObj = {
    now() {},
    mark() {}
  };
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
module.exports = {
  _PerfService,
  perfService
};

/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {const React = __webpack_require__(1);
const ReactDOM = __webpack_require__(13);
const Base = __webpack_require__(14);
const { Provider } = __webpack_require__(3);
const initStore = __webpack_require__(31);
const { reducers } = __webpack_require__(6);
const DetectUserSessionStart = __webpack_require__(33);
const { addSnippetsSubscriber } = __webpack_require__(34);
const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);

const store = initStore(reducers, global.gActivityStreamPrerenderedState);

new DetectUserSessionStart(store).sendEventOrAddListener();

// If we are starting in a prerendered state, we must wait until the first render
// to request state rehydration (see Base.jsx). If we are NOT in a prerendered state,
// we can request it immedately.
if (!global.gActivityStreamPrerenderedState) {
  store.dispatch(ac.SendToMain({ type: at.NEW_TAB_STATE_REQUEST }));
}

ReactDOM.render(React.createElement(
  Provider,
  { store: store },
  React.createElement(Base, {
    isPrerendered: !!global.gActivityStreamPrerenderedState,
    locale: global.document.documentElement.lang,
    strings: global.gActivityStreamStrings })
), document.getElementById("root"));

addSnippetsSubscriber(store);
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 13 */
/***/ (function(module, exports) {

module.exports = ReactDOM;

/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { addLocaleData, IntlProvider } = __webpack_require__(2);
const TopSites = __webpack_require__(15);
const Search = __webpack_require__(21);
const ConfirmDialog = __webpack_require__(23);
const ManualMigration = __webpack_require__(24);
const PreferencesPane = __webpack_require__(25);
const Sections = __webpack_require__(26);
const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);
const { PrerenderData } = __webpack_require__(30);

// Add the locale data for pluralization and relative-time formatting for now,
// this just uses english locale data. We can make this more sophisticated if
// more features are needed.
function addLocaleDataForReactIntl(locale) {
  addLocaleData([{ locale, parentLocale: "en" }]);
}

class Base extends React.PureComponent {
  componentWillMount() {
    const { App, locale } = this.props;
    this.sendNewTabRehydrated(App);
    addLocaleDataForReactIntl(locale);
  }

  componentDidMount() {
    // Request state AFTER the first render to ensure we don't cause the
    // prerendered DOM to be unmounted. Otherwise, NEW_TAB_STATE_REQUEST is
    // dispatched right after the store is ready.
    if (this.props.isPrerendered) {
      this.props.dispatch(ac.SendToMain({ type: at.NEW_TAB_STATE_REQUEST }));
      this.props.dispatch(ac.SendToMain({ type: at.PAGE_PRERENDERED }));
    }
  }

  componentWillUpdate({ App }) {
    this.sendNewTabRehydrated(App);
  }

  // The NEW_TAB_REHYDRATED event is used to inform feeds that their
  // data has been consumed e.g. for counting the number of tabs that
  // have rendered that data.
  sendNewTabRehydrated(App) {
    if (App && App.initialized && !this.renderNotified) {
      this.props.dispatch(ac.SendToMain({ type: at.NEW_TAB_REHYDRATED, data: {} }));
      this.renderNotified = true;
    }
  }

  render() {
    const props = this.props;
    const { App, locale, strings } = props;
    const { initialized } = App;
    const prefs = props.Prefs.values;

    const shouldBeFixedToTop = PrerenderData.arePrefsValid(name => prefs[name]);

    const outerClassName = `outer-wrapper${shouldBeFixedToTop ? " fixed-to-top" : ""}`;

    if (!props.isPrerendered && !initialized) {
      return null;
    }

    return React.createElement(
      IntlProvider,
      { locale: locale, messages: strings },
      React.createElement(
        "div",
        { className: outerClassName },
        React.createElement(
          "main",
          null,
          prefs.showSearch && React.createElement(Search, null),
          React.createElement(
            "div",
            { className: `body-wrapper${initialized ? " on" : ""}` },
            !prefs.migrationExpired && React.createElement(ManualMigration, null),
            prefs.showTopSites && React.createElement(TopSites, null),
            React.createElement(Sections, null)
          ),
          React.createElement(ConfirmDialog, null)
        ),
        initialized && React.createElement(PreferencesPane, null)
      )
    );
  }
}

module.exports = connect(state => ({ App: state.App, Prefs: state.Prefs }))(Base);
module.exports._unconnected = Base;

/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { FormattedMessage } = __webpack_require__(2);

const TopSitesEdit = __webpack_require__(16);
const { TopSiteList } = __webpack_require__(7);
const CollapsibleSection = __webpack_require__(9);
const ComponentPerfTimer = __webpack_require__(10);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);
const { MIN_RICH_FAVICON_SIZE, MIN_CORNER_FAVICON_SIZE } = __webpack_require__(5);

/**
 * Iterates through TopSites and counts types of images.
 * @param acc Accumulator for reducer.
 * @param topsite Entry in TopSites.
 */
function countTopSitesIconsTypes(topSites) {
  const countTopSitesTypes = (acc, link) => {
    if (link.tippyTopIcon || link.faviconRef === "tippytop") {
      acc.tippytop++;
    } else if (link.faviconSize >= MIN_RICH_FAVICON_SIZE) {
      acc.rich_icon++;
    } else if (link.screenshot && link.faviconSize >= MIN_CORNER_FAVICON_SIZE) {
      acc.screenshot_with_icon++;
    } else if (link.screenshot) {
      acc.screenshot++;
    } else {
      acc.no_image++;
    }

    return acc;
  };

  return topSites.reduce(countTopSitesTypes, {
    "screenshot_with_icon": 0,
    "screenshot": 0,
    "tippytop": 0,
    "rich_icon": 0,
    "no_image": 0
  });
}

class TopSites extends React.PureComponent {
  /**
   * Dispatch session statistics about the quality of TopSites icons.
   */
  _dispatchTopSitesIconStats() {
    const topSitesIconsStats = countTopSitesIconsTypes(this._getTopSites());
    // Dispatch telemetry event with the count of TopSites images types.
    this.props.dispatch(ac.SendToMain({
      type: at.SAVE_SESSION_PERF_DATA,
      data: { topsites_icon_stats: topSitesIconsStats }
    }));
  }

  /**
   * Return the TopSites to display based on prefs.
   */
  _getTopSites() {
    return this.props.TopSites.rows.slice(0, this.props.TopSitesCount);
  }

  componentDidUpdate() {
    this._dispatchTopSitesIconStats();
  }

  componentDidMount() {
    this._dispatchTopSitesIconStats();
  }

  render() {
    const props = this.props;
    const infoOption = {
      header: { id: "settings_pane_topsites_header" },
      body: { id: "settings_pane_topsites_body" }
    };
    return React.createElement(
      ComponentPerfTimer,
      { id: "topsites", initialized: props.TopSites.initialized, dispatch: props.dispatch },
      React.createElement(
        CollapsibleSection,
        { className: "top-sites", icon: "topsites", title: React.createElement(FormattedMessage, { id: "header_top_sites" }), infoOption: infoOption, prefName: "collapseTopSites", Prefs: props.Prefs, dispatch: props.dispatch },
        React.createElement(TopSiteList, { TopSites: props.TopSites, TopSitesCount: props.TopSitesCount, dispatch: props.dispatch, intl: props.intl }),
        React.createElement(TopSitesEdit, props)
      )
    );
  }
}

module.exports = connect(state => ({ TopSites: state.TopSites, Prefs: state.Prefs, TopSitesCount: state.Prefs.values.topSitesCount }))(TopSites);
module.exports._unconnected = TopSites;

/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { FormattedMessage, injectIntl } = __webpack_require__(2);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);

const TopSiteForm = __webpack_require__(17);
const { TopSiteList } = __webpack_require__(7);

const { TOP_SITES_DEFAULT_LENGTH, TOP_SITES_SHOWMORE_LENGTH } = __webpack_require__(6);
const { TOP_SITES_SOURCE } = __webpack_require__(5);

class TopSitesEdit extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      showEditModal: false,
      showAddForm: false,
      showEditForm: false,
      editIndex: -1 // Index of top site being edited
    };
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
    this.onShowMoreLessClick = this.onShowMoreLessClick.bind(this);
    this.onModalOverlayClick = this.onModalOverlayClick.bind(this);
    this.onAddButtonClick = this.onAddButtonClick.bind(this);
    this.onFormClose = this.onFormClose.bind(this);
    this.onEdit = this.onEdit.bind(this);
  }
  onEditButtonClick() {
    this.setState({ showEditModal: !this.state.showEditModal });
    const event = this.state.showEditModal ? "TOP_SITES_EDIT_OPEN" : "TOP_SITES_EDIT_CLOSE";
    this.props.dispatch(ac.UserEvent({
      source: TOP_SITES_SOURCE,
      event
    }));
  }
  onModalOverlayClick() {
    this.setState({ showEditModal: false, showAddForm: false, showEditForm: false });
    this.props.dispatch(ac.UserEvent({
      source: TOP_SITES_SOURCE,
      event: "TOP_SITES_EDIT_CLOSE"
    }));
    this.props.dispatch({ type: at.TOP_SITES_CANCEL_EDIT });
  }
  onShowMoreLessClick() {
    const prefIsSetToDefault = this.props.TopSitesCount === TOP_SITES_DEFAULT_LENGTH;
    this.props.dispatch(ac.SendToMain({
      type: at.SET_PREF,
      data: { name: "topSitesCount", value: prefIsSetToDefault ? TOP_SITES_SHOWMORE_LENGTH : TOP_SITES_DEFAULT_LENGTH }
    }));
    this.props.dispatch(ac.UserEvent({
      source: TOP_SITES_SOURCE,
      event: prefIsSetToDefault ? "TOP_SITES_EDIT_SHOW_MORE" : "TOP_SITES_EDIT_SHOW_LESS"
    }));
  }
  onAddButtonClick() {
    this.setState({ showAddForm: true });
    this.props.dispatch(ac.UserEvent({
      source: TOP_SITES_SOURCE,
      event: "TOP_SITES_ADD_FORM_OPEN"
    }));
  }
  onFormClose() {
    this.setState({ showAddForm: false, showEditForm: false });
    this.props.dispatch({ type: at.TOP_SITES_CANCEL_EDIT });
  }
  onEdit(index) {
    this.setState({ showEditForm: true, editIndex: index });
    this.props.dispatch(ac.UserEvent({
      source: TOP_SITES_SOURCE,
      event: "TOP_SITES_EDIT_FORM_OPEN"
    }));
  }
  render() {
    const showEditForm = this.props.TopSites.editForm && this.props.TopSites.editForm.visible || this.state.showEditModal && this.state.showEditForm;
    let editIndex = this.state.editIndex;
    if (showEditForm && this.props.TopSites.editForm.visible) {
      const targetURL = this.props.TopSites.editForm.site.url;
      editIndex = this.props.TopSites.rows.findIndex(s => s.url === targetURL);
    }
    return React.createElement(
      "div",
      { className: "edit-topsites-wrapper" },
      React.createElement(
        "div",
        { className: "edit-topsites-button" },
        React.createElement(
          "button",
          {
            className: "edit",
            title: this.props.intl.formatMessage({ id: "edit_topsites_button_label" }),
            onClick: this.onEditButtonClick },
          React.createElement(FormattedMessage, { id: "edit_topsites_button_text" })
        )
      ),
      this.state.showEditModal && !this.state.showAddForm && !this.state.showEditForm && React.createElement(
        "div",
        { className: "edit-topsites" },
        React.createElement("div", { className: "modal-overlay", onClick: this.onModalOverlayClick }),
        React.createElement(
          "div",
          { className: "modal" },
          React.createElement(
            "section",
            { className: "edit-topsites-inner-wrapper" },
            React.createElement(
              "div",
              { className: "section-top-bar" },
              React.createElement(
                "h3",
                { className: "section-title" },
                React.createElement("span", { className: `icon icon-small-spacer icon-topsites` }),
                React.createElement(FormattedMessage, { id: "header_top_sites" })
              )
            ),
            React.createElement(TopSiteList, { TopSites: this.props.TopSites, TopSitesCount: this.props.TopSitesCount, onEdit: this.onEdit, dispatch: this.props.dispatch, intl: this.props.intl })
          ),
          React.createElement(
            "section",
            { className: "actions" },
            React.createElement(
              "button",
              { className: "add", onClick: this.onAddButtonClick },
              React.createElement(FormattedMessage, { id: "edit_topsites_add_button" })
            ),
            React.createElement(
              "button",
              { className: `icon icon-topsites show-${this.props.TopSitesCount === TOP_SITES_DEFAULT_LENGTH ? "more" : "less"}`, onClick: this.onShowMoreLessClick },
              React.createElement(FormattedMessage, { id: `edit_topsites_show${this.props.TopSitesCount === TOP_SITES_DEFAULT_LENGTH ? "more" : "less"}_button` })
            ),
            React.createElement(
              "button",
              { className: "done", onClick: this.onEditButtonClick },
              React.createElement(FormattedMessage, { id: "edit_topsites_done_button" })
            )
          )
        )
      ),
      this.state.showEditModal && this.state.showAddForm && React.createElement(
        "div",
        { className: "edit-topsites" },
        React.createElement("div", { className: "modal-overlay", onClick: this.onModalOverlayClick }),
        React.createElement(
          "div",
          { className: "modal" },
          React.createElement(TopSiteForm, { onClose: this.onFormClose, dispatch: this.props.dispatch, intl: this.props.intl })
        )
      ),
      showEditForm && React.createElement(
        "div",
        { className: "edit-topsites" },
        React.createElement("div", { className: "modal-overlay", onClick: this.onModalOverlayClick }),
        React.createElement(
          "div",
          { className: "modal" },
          React.createElement(TopSiteForm, {
            label: this.props.TopSites.rows[editIndex].label || this.props.TopSites.rows[editIndex].hostname,
            url: this.props.TopSites.rows[editIndex].url,
            index: editIndex,
            editMode: true,
            onClose: this.onFormClose,
            dispatch: this.props.dispatch,
            intl: this.props.intl })
        )
      )
    );
  }
}

module.exports = injectIntl(TopSitesEdit);
module.exports._unconnected = TopSitesEdit;

/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);
const { FormattedMessage } = __webpack_require__(2);

const { TOP_SITES_SOURCE } = __webpack_require__(5);

class TopSiteForm extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      label: props.label || "",
      url: props.url || "",
      validationError: false
    };
    this.onLabelChange = this.onLabelChange.bind(this);
    this.onUrlChange = this.onUrlChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onAddButtonClick = this.onAddButtonClick.bind(this);
    this.onSaveButtonClick = this.onSaveButtonClick.bind(this);
    this.onUrlInputMount = this.onUrlInputMount.bind(this);
  }
  onLabelChange(event) {
    this.resetValidation();
    this.setState({ "label": event.target.value });
  }
  onUrlChange(event) {
    this.resetValidation();
    this.setState({ "url": event.target.value });
  }
  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }
  onAddButtonClick(ev) {
    ev.preventDefault();
    if (this.validateForm()) {
      let site = { url: this.cleanUrl() };
      if (this.state.label !== "") {
        site.label = this.state.label;
      }
      this.props.dispatch(ac.SendToMain({
        type: at.TOP_SITES_ADD,
        data: { site }
      }));
      this.props.dispatch(ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "TOP_SITES_ADD"
      }));
      this.props.onClose();
    }
  }
  onSaveButtonClick(ev) {
    ev.preventDefault();
    if (this.validateForm()) {
      let site = { url: this.cleanUrl() };
      if (this.state.label !== "") {
        site.label = this.state.label;
      }
      this.props.dispatch(ac.SendToMain({
        type: at.TOP_SITES_PIN,
        data: { site, index: this.props.index }
      }));
      this.props.dispatch(ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "TOP_SITES_EDIT",
        action_position: this.props.index
      }));
      this.props.onClose();
    }
  }
  cleanUrl() {
    let url = this.state.url;
    // If we are missing a protocol, prepend http://
    if (!url.startsWith("http:") && !url.startsWith("https:")) {
      url = `http://${url}`;
    }
    return url;
  }
  resetValidation() {
    if (this.state.validationError) {
      this.setState({ validationError: false });
    }
  }
  validateUrl() {
    try {
      return !!new URL(this.cleanUrl());
    } catch (e) {
      return false;
    }
  }
  validateForm() {
    this.resetValidation();
    // Only the URL is required and must be valid.
    if (!this.state.url || !this.validateUrl()) {
      this.setState({ validationError: true });
      this.inputUrl.focus();
      return false;
    }
    return true;
  }
  onUrlInputMount(input) {
    this.inputUrl = input;
  }
  render() {
    return React.createElement(
      "form",
      { className: "topsite-form" },
      React.createElement(
        "section",
        { className: "edit-topsites-inner-wrapper" },
        React.createElement(
          "div",
          { className: "form-wrapper" },
          React.createElement(
            "h3",
            { className: "section-title" },
            React.createElement(FormattedMessage, { id: this.props.editMode ? "topsites_form_edit_header" : "topsites_form_add_header" })
          ),
          React.createElement(
            "div",
            { className: "field title" },
            React.createElement("input", {
              type: "text",
              value: this.state.label,
              onChange: this.onLabelChange,
              placeholder: this.props.intl.formatMessage({ id: "topsites_form_title_placeholder" }) })
          ),
          React.createElement(
            "div",
            { className: `field url${this.state.validationError ? " invalid" : ""}` },
            React.createElement("input", {
              type: "text",
              ref: this.onUrlInputMount,
              value: this.state.url,
              onChange: this.onUrlChange,
              placeholder: this.props.intl.formatMessage({ id: "topsites_form_url_placeholder" }) }),
            this.state.validationError && React.createElement(
              "aside",
              { className: "error-tooltip" },
              React.createElement(FormattedMessage, { id: "topsites_form_url_validation" })
            )
          )
        )
      ),
      React.createElement(
        "section",
        { className: "actions" },
        React.createElement(
          "button",
          { className: "cancel", type: "button", onClick: this.onCancelButtonClick },
          React.createElement(FormattedMessage, { id: "topsites_form_cancel_button" })
        ),
        this.props.editMode && React.createElement(
          "button",
          { className: "done save", type: "submit", onClick: this.onSaveButtonClick },
          React.createElement(FormattedMessage, { id: "topsites_form_save_button" })
        ),
        !this.props.editMode && React.createElement(
          "button",
          { className: "done add", type: "submit", onClick: this.onAddButtonClick },
          React.createElement(FormattedMessage, { id: "topsites_form_add_button" })
        )
      )
    );
  }
}

TopSiteForm.defaultProps = {
  label: "",
  url: "",
  index: 0,
  editMode: false // by default we are in "Add New Top Site" mode
};

module.exports = TopSiteForm;

/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);

class ContextMenu extends React.PureComponent {
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

class ContextMenuItem extends React.PureComponent {
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
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);

/**
 * List of functions that return items that can be included as menu options in a
 * LinkMenu. All functions take the site as the first parameter, and optionally
 * the index of the site.
 */
module.exports = {
  Separator: () => ({ type: "separator" }),
  RemoveBookmark: site => ({
    id: "menu_action_remove_bookmark",
    icon: "bookmark-added",
    action: ac.SendToMain({
      type: at.DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "menu_action_bookmark",
    icon: "bookmark-hollow",
    action: ac.SendToMain({
      type: at.BOOKMARK_URL,
      data: { url: site.url, title: site.title, type: site.type }
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
  BlockUrl: (site, index, eventSource) => ({
    id: "menu_action_dismiss",
    icon: "dismiss",
    action: ac.SendToMain({
      type: at.BLOCK_URL,
      data: site.url
    }),
    impression: ac.ImpressionStats({
      source: eventSource,
      block: 0,
      tiles: [{ id: site.guid, pos: index }]
    }),
    userEvent: "BLOCK"
  }),
  DeleteUrl: site => ({
    id: "menu_action_delete",
    icon: "delete",
    action: {
      type: at.DIALOG_OPEN,
      data: {
        onConfirm: [ac.SendToMain({ type: at.DELETE_HISTORY_URL, data: { url: site.url, forceBlock: site.bookmarkGuid } }), ac.UserEvent({ event: "DELETE" })],
        body_string_id: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
        confirm_button_string_id: "menu_action_delete",
        cancel_button_string_id: "topsites_form_cancel_button",
        icon: "modal-delete"
      }
    },
    userEvent: "DIALOG_OPEN"
  }),
  PinTopSite: (site, index) => ({
    id: "menu_action_pin",
    icon: "pin",
    action: ac.SendToMain({
      type: at.TOP_SITES_PIN,
      data: { site: { url: site.url }, index }
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
  SaveToPocket: (site, index, eventSource) => ({
    id: "menu_action_save_to_pocket",
    icon: "pocket",
    action: ac.SendToMain({
      type: at.SAVE_TO_POCKET,
      data: { site: { url: site.url, title: site.title } }
    }),
    impression: ac.ImpressionStats({
      source: eventSource,
      pocket: 0,
      tiles: [{ id: site.guid, pos: index }]
    }),
    userEvent: "SAVE_TO_POCKET"
  }),
  EditTopSite: site => ({
    id: "edit_topsites_button_text",
    icon: "edit",
    action: {
      type: at.TOP_SITES_EDIT,
      data: { url: site.url, label: site.label }
    }
  })
};

module.exports.CheckBookmark = site => site.bookmarkGuid ? module.exports.RemoveBookmark(site) : module.exports.AddBookmark(site);
module.exports.CheckPinTopSite = (site, index) => site.isPinned ? module.exports.UnpinTopSite(site) : module.exports.PinTopSite(site, index);

/***/ }),
/* 20 */
/***/ (function(module, exports) {

var Dedupe = class Dedupe {
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
};
module.exports = {
  Dedupe
};

/***/ }),
/* 21 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* globals ContentSearchUIController */


const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { FormattedMessage, injectIntl } = __webpack_require__(2);
const { actionCreators: ac } = __webpack_require__(0);
const { IS_NEWTAB } = __webpack_require__(22);

class Search extends React.PureComponent {
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
      const healthReportKey = IS_NEWTAB ? "newtab" : "abouthome";

      // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)
      const searchSource = IS_NEWTAB ? "newtab" : "homepage";

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
    return React.createElement(
      "div",
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
          id: "searchSubmit",
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
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {module.exports = {
  // constant to know if the page is about:newtab or about:home
  IS_NEWTAB: global.document && global.document.documentURI === "about:newtab"
};
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { FormattedMessage } = __webpack_require__(2);
const { actionTypes, actionCreators: ac } = __webpack_require__(0);

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
class ConfirmDialog extends React.PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({ type: actionTypes.DIALOG_CANCEL });
    this.props.dispatch(ac.UserEvent({ event: actionTypes.DIALOG_CANCEL }));
  }

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  }

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
  }

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
        { className: "modal" },
        React.createElement(
          "section",
          { className: "modal-message" },
          this.props.data.icon && React.createElement("span", { className: `icon icon-spacer icon-${this.props.data.icon}` }),
          this._renderModalMessage()
        ),
        React.createElement(
          "section",
          { className: "actions" },
          React.createElement(
            "button",
            { onClick: this._handleCancelBtn },
            React.createElement(FormattedMessage, { id: this.props.data.cancel_button_string_id })
          ),
          React.createElement(
            "button",
            { className: "done", onClick: this._handleConfirmBtn },
            React.createElement(FormattedMessage, { id: this.props.data.confirm_button_string_id })
          )
        )
      )
    );
  }
}

module.exports = connect(state => state.Dialog)(ConfirmDialog);
module.exports._unconnected = ConfirmDialog;
module.exports.Dialog = ConfirmDialog;

/***/ }),
/* 24 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { FormattedMessage } = __webpack_require__(2);
const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);

/**
 * Manual migration component used to start the profile import wizard.
 * Message is presented temporarily and will go away if:
 * 1.  User clicks "No Thanks"
 * 2.  User completed the data import
 * 3.  After 3 active days
 * 4.  User clicks "Cancel" on the import wizard (currently not implemented).
 */
class ManualMigration extends React.PureComponent {
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
        React.createElement("span", { className: "icon icon-import" }),
        React.createElement(FormattedMessage, { id: "manual_migration_explanation2" })
      ),
      React.createElement(
        "div",
        { className: "manual-migration-actions actions" },
        React.createElement(
          "button",
          { className: "dismiss", onClick: this.onCancelTour },
          React.createElement(FormattedMessage, { id: "manual_migration_cancel_button" })
        ),
        React.createElement(
          "button",
          { onClick: this.onLaunchTour },
          React.createElement(FormattedMessage, { id: "manual_migration_import_button" })
        )
      )
    );
  }
}

module.exports = connect()(ManualMigration);
module.exports._unconnected = ManualMigration;

/***/ }),
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { injectIntl, FormattedMessage } = __webpack_require__(2);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);
const { TOP_SITES_DEFAULT_LENGTH, TOP_SITES_SHOWMORE_LENGTH } = __webpack_require__(6);

const getFormattedMessage = message => typeof message === "string" ? React.createElement(
  "span",
  null,
  message
) : React.createElement(FormattedMessage, message);

const PreferencesInput = props => React.createElement(
  "section",
  null,
  React.createElement("input", { type: "checkbox", id: props.prefName, name: props.prefName, checked: props.value, disabled: props.disabled, onChange: props.onChange, className: props.className }),
  React.createElement(
    "label",
    { htmlFor: props.prefName, className: props.labelClassName },
    getFormattedMessage(props.titleString)
  ),
  props.descString && React.createElement(
    "p",
    { className: "prefs-input-description" },
    getFormattedMessage(props.descString)
  ),
  React.Children.map(props.children, child => React.createElement(
    "div",
    { className: `options${child.props.disabled ? " disabled" : ""}` },
    child
  ))
);

class PreferencesPane extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleClickOutside = this.handleClickOutside.bind(this);
    this.handlePrefChange = this.handlePrefChange.bind(this);
    this.handleSectionChange = this.handleSectionChange.bind(this);
    this.togglePane = this.togglePane.bind(this);
    this.onWrapperMount = this.onWrapperMount.bind(this);
  }
  componentDidUpdate(prevProps, prevState) {
    if (prevProps.PreferencesPane.visible !== this.props.PreferencesPane.visible) {
      // While the sidebar is open, listen for all document clicks.
      if (this.isSidebarOpen()) {
        document.addEventListener("click", this.handleClickOutside);
      } else {
        document.removeEventListener("click", this.handleClickOutside);
      }
    }
  }
  isSidebarOpen() {
    return this.props.PreferencesPane.visible;
  }
  handleClickOutside(event) {
    // if we are showing the sidebar and there is a click outside, close it.
    if (this.isSidebarOpen() && !this.wrapper.contains(event.target)) {
      this.togglePane();
    }
  }
  handlePrefChange(event) {
    const target = event.target;
    const { name, checked } = target;
    let value = checked;
    if (name === "topSitesCount") {
      value = checked ? TOP_SITES_SHOWMORE_LENGTH : TOP_SITES_DEFAULT_LENGTH;
    }
    this.props.dispatch(ac.SetPref(name, value));
  }
  handleSectionChange(event) {
    const target = event.target;
    const id = target.name;
    const type = target.checked ? at.SECTION_ENABLE : at.SECTION_DISABLE;
    this.props.dispatch(ac.SendToMain({ type, data: id }));
  }
  togglePane() {
    if (this.isSidebarOpen()) {
      this.props.dispatch({ type: at.SETTINGS_CLOSE });
      this.props.dispatch(ac.UserEvent({ event: "CLOSE_NEWTAB_PREFS" }));
    } else {
      this.props.dispatch({ type: at.SETTINGS_OPEN });
      this.props.dispatch(ac.UserEvent({ event: "OPEN_NEWTAB_PREFS" }));
    }
  }
  onWrapperMount(wrapper) {
    this.wrapper = wrapper;
  }
  render() {
    const props = this.props;
    const prefs = props.Prefs.values;
    const sections = props.Sections;
    const isVisible = this.isSidebarOpen();
    return React.createElement(
      "div",
      { className: "prefs-pane-wrapper", ref: this.onWrapperMount },
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
              React.createElement(FormattedMessage, { id: "settings_pane_body2" })
            ),
            React.createElement(PreferencesInput, {
              className: "showSearch",
              prefName: "showSearch",
              value: prefs.showSearch,
              onChange: this.handlePrefChange,
              titleString: { id: "settings_pane_search_header" },
              descString: { id: "settings_pane_search_body" } }),
            React.createElement("hr", null),
            React.createElement(
              PreferencesInput,
              {
                className: "showTopSites",
                prefName: "showTopSites",
                value: prefs.showTopSites,
                onChange: this.handlePrefChange,
                titleString: { id: "settings_pane_topsites_header" },
                descString: { id: "settings_pane_topsites_body" } },
              React.createElement(PreferencesInput, {
                className: "showMoreTopSites",
                prefName: "topSitesCount",
                disabled: !prefs.showTopSites,
                value: prefs.topSitesCount !== TOP_SITES_DEFAULT_LENGTH,
                onChange: this.handlePrefChange,
                titleString: { id: "settings_pane_topsites_options_showmore" },
                labelClassName: "icon icon-topsites" })
            ),
            sections.filter(section => !section.shouldHidePref).map(({ id, title, enabled, pref }) => React.createElement(
              PreferencesInput,
              {
                key: id,
                className: "showSection",
                prefName: pref && pref.feed || id,
                value: enabled,
                onChange: pref && pref.feed ? this.handlePrefChange : this.handleSectionChange,
                titleString: pref && pref.titleString || title,
                descString: pref && pref.descString },
              pref.nestedPrefs && pref.nestedPrefs.map(nestedPref => React.createElement(PreferencesInput, {
                key: nestedPref.name,
                prefName: nestedPref.name,
                disabled: !enabled,
                value: prefs[nestedPref.name],
                onChange: this.handlePrefChange,
                titleString: nestedPref.titleString,
                labelClassName: `icon ${nestedPref.icon}` }))
            )),
            !prefs.disableSnippets && React.createElement("hr", null),
            !prefs.disableSnippets && React.createElement(PreferencesInput, { className: "showSnippets", prefName: "feeds.snippets",
              value: prefs["feeds.snippets"], onChange: this.handlePrefChange,
              titleString: { id: "settings_pane_snippets_header" },
              descString: { id: "settings_pane_snippets_body" } })
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

module.exports = connect(state => ({ Prefs: state.Prefs, PreferencesPane: state.PreferencesPane, Sections: state.Sections }))(injectIntl(PreferencesPane));
module.exports.PreferencesPane = PreferencesPane;
module.exports.PreferencesInput = PreferencesInput;

/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

const React = __webpack_require__(1);
const { connect } = __webpack_require__(3);
const { injectIntl, FormattedMessage } = __webpack_require__(2);
const Card = __webpack_require__(27);
const { PlaceholderCard } = Card;
const Topics = __webpack_require__(29);
const { actionCreators: ac } = __webpack_require__(0);
const CollapsibleSection = __webpack_require__(9);
const ComponentPerfTimer = __webpack_require__(10);

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const CARDS_PER_ROW = 3;

function getFormattedMessage(message) {
  return typeof message === "string" ? React.createElement(
    "span",
    null,
    message
  ) : React.createElement(FormattedMessage, message);
}

class Section extends React.PureComponent {
  _dispatchImpressionStats() {
    const { props } = this;
    const maxCards = 3 * props.maxRows;
    const cards = props.rows.slice(0, maxCards);

    if (this.needsImpressionStats(cards)) {
      props.dispatch(ac.ImpressionStats({
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

      // When the page becoems visible, send the impression stats ping if the section isn't collapsed.
      this._onVisibilityChange = () => {
        if (props.document.visibilityState === VISIBLE) {
          const { id, Prefs } = this.props;
          const isCollapsed = Prefs.values[`section.${id}.collapsed`];
          if (!isCollapsed) {
            this._dispatchImpressionStats();
          }
          props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };
      props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  componentDidMount() {
    const { id, rows, Prefs } = this.props;
    const isCollapsed = Prefs.values[`section.${id}.collapsed`];
    if (rows.length && !isCollapsed) {
      this.sendImpressionStatsOrAddListener();
    }
  }

  componentDidUpdate(prevProps) {
    const { props } = this;
    const { id, Prefs } = props;
    const isCollapsedPref = `section.${id}.collapsed`;
    const isCollapsed = Prefs.values[isCollapsedPref];
    const wasCollapsed = prevProps.Prefs.values[isCollapsedPref];
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
      infoOption, emptyState, dispatch, maxRows,
      contextMenuOptions, initialized, disclaimer
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
    return React.createElement(
      ComponentPerfTimer,
      this.props,
      React.createElement(
        CollapsibleSection,
        { className: "section", icon: icon, title: getFormattedMessage(title),
          infoOption: infoOption,
          id: id,
          eventSource: eventSource,
          disclaimer: disclaimer,
          prefName: `section.${id}.collapsed`,
          Prefs: this.props.Prefs,
          dispatch: this.props.dispatch },
        !shouldShowEmptyState && React.createElement(
          "ul",
          { className: "section-list", style: { padding: 0 } },
          realRows.map((link, index) => link && React.createElement(Card, { key: index, index: index, dispatch: dispatch, link: link, contextMenuOptions: contextMenuOptions,
            eventSource: eventSource, shouldSendImpressionStats: this.props.shouldSendImpressionStats })),
          placeholders > 0 && [...new Array(placeholders)].map((_, i) => React.createElement(PlaceholderCard, { key: i }))
        ),
        shouldShowEmptyState && React.createElement(
          "div",
          { className: "section-empty-state" },
          React.createElement(
            "div",
            { className: "empty-state" },
            emptyState.icon && emptyState.icon.startsWith("moz-extension://") ? React.createElement("img", { className: "empty-state-icon icon", style: { "background-image": `url('${emptyState.icon}')` } }) : React.createElement("img", { className: `empty-state-icon icon icon-${emptyState.icon}` }),
            React.createElement(
              "p",
              { className: "empty-state-message" },
              getFormattedMessage(emptyState.message)
            )
          )
        ),
        shouldShowTopics && React.createElement(Topics, { topics: this.props.topics, read_more_endpoint: this.props.read_more_endpoint })
      )
    );
  }
}

Section.defaultProps = {
  document: global.document,
  rows: [],
  emptyState: {},
  title: ""
};

const SectionIntl = injectIntl(Section);

class Sections extends React.PureComponent {
  render() {
    const sections = this.props.Sections;
    return React.createElement(
      "div",
      { className: "sections-list" },
      sections.filter(section => section.enabled).map(section => React.createElement(SectionIntl, _extends({ key: section.id }, section, { Prefs: this.props.Prefs, dispatch: this.props.dispatch })))
    );
  }
}

module.exports = connect(state => ({ Sections: state.Sections, Prefs: state.Prefs }))(Sections);
module.exports._unconnected = Sections;
module.exports.SectionIntl = SectionIntl;
module.exports._unconnectedSection = Section;
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 27 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const LinkMenu = __webpack_require__(8);
const { FormattedMessage } = __webpack_require__(2);
const cardContextTypes = __webpack_require__(28);
const { actionCreators: ac, actionTypes: at } = __webpack_require__(0);

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
class Card extends React.PureComponent {
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
  onLinkClick(event) {
    event.preventDefault();
    const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
    this.props.dispatch(ac.SendToMain({
      type: at.OPEN_LINK,
      data: Object.assign(this.props.link, { event: { altKey, button, ctrlKey, metaKey, shiftKey } })
    }));
    this.props.dispatch(ac.UserEvent({
      event: "CLICK",
      source: this.props.eventSource,
      action_position: this.props.index
    }));
    if (this.props.shouldSendImpressionStats) {
      this.props.dispatch(ac.ImpressionStats({
        source: this.props.eventSource,
        click: 0,
        tiles: [{ id: this.props.link.guid, pos: this.props.index }]
      }));
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

    return React.createElement(
      "li",
      { className: `card-outer${isContextMenuOpen ? " active" : ""}${props.placeholder ? " placeholder" : ""}` },
      React.createElement(
        "a",
        { href: link.url, onClick: !props.placeholder && this.onLinkClick },
        React.createElement(
          "div",
          { className: "card" },
          hasImage && React.createElement(
            "div",
            { className: "card-preview-image-outer" },
            React.createElement("div", { className: `card-preview-image${this.state.imageLoaded ? " loaded" : ""}`, style: imageStyle })
          ),
          React.createElement(
            "div",
            { className: `card-details${hasImage ? "" : " no-image"}` },
            link.hostname && React.createElement(
              "div",
              { className: "card-host-name" },
              link.hostname
            ),
            React.createElement(
              "div",
              { className: ["card-text", icon ? "" : "no-context", link.description ? "" : "no-description", link.hostname ? "" : "no-host-name", hasImage ? "" : "no-image"].join(" ") },
              React.createElement(
                "h4",
                { className: "card-title", dir: "auto" },
                link.title
              ),
              React.createElement(
                "p",
                { className: "card-description", dir: "auto" },
                link.description
              )
            ),
            React.createElement(
              "div",
              { className: "card-context" },
              icon && !link.context && React.createElement("span", { className: `card-context-icon icon icon-${icon}` }),
              link.icon && link.context && React.createElement("span", { className: "card-context-icon icon", style: { backgroundImage: `url('${link.icon}')` } }),
              intlID && !link.context && React.createElement(
                "div",
                { className: "card-context-label" },
                React.createElement(FormattedMessage, { id: intlID, defaultMessage: "Visited" })
              ),
              link.context && React.createElement(
                "div",
                { className: "card-context-label" },
                link.context
              )
            )
          )
        )
      ),
      !props.placeholder && React.createElement(
        "button",
        { className: "context-menu-button icon",
          onClick: this.onMenuButtonClick },
        React.createElement(
          "span",
          { className: "sr-only" },
          `Open context menu for ${link.title}`
        )
      ),
      !props.placeholder && React.createElement(LinkMenu, {
        dispatch: dispatch,
        index: index,
        source: eventSource,
        onUpdate: this.onMenuUpdate,
        options: link.contextMenuOptions || contextMenuOptions,
        site: link,
        visible: isContextMenuOpen,
        shouldSendImpressionStats: shouldSendImpressionStats })
    );
  }
}
Card.defaultProps = { link: {} };

const PlaceholderCard = () => React.createElement(Card, { placeholder: true });

module.exports = Card;
module.exports.PlaceholderCard = PlaceholderCard;

/***/ }),
/* 28 */
/***/ (function(module, exports) {

module.exports = {
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
  }
};

/***/ }),
/* 29 */
/***/ (function(module, exports, __webpack_require__) {

const React = __webpack_require__(1);
const { FormattedMessage } = __webpack_require__(2);

class Topic extends React.PureComponent {
  render() {
    const { url, name } = this.props;
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

class Topics extends React.PureComponent {
  render() {
    const { topics, read_more_endpoint } = this.props;
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
        topics && topics.map(t => React.createElement(Topic, { key: t.name, url: t.url, name: t.name }))
      ),
      read_more_endpoint && React.createElement(
        "a",
        { className: "topic-read-more", href: read_more_endpoint },
        React.createElement(FormattedMessage, { id: "pocket_read_even_more" })
      )
    );
  }
}

module.exports = Topics;
module.exports._unconnected = Topics;
module.exports.Topic = Topic;

/***/ }),
/* 30 */
/***/ (function(module, exports) {

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
      }
      throw new Error("Your validation configuration is not properly configured");
    }, []);
  }

  arePrefsValid(getPref) {
    for (const prefs of this.validation) {
      // {oneOf: ["foo", "bar"]}
      if (prefs && prefs.oneOf && !prefs.oneOf.some(name => getPref(name) === this.initialPrefs[name])) {
        return false;

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
    "showTopSites": true,
    "showSearch": true,
    "topSitesCount": 6,
    "collapseTopSites": false,
    "section.highlights.collapsed": false,
    "section.topstories.collapsed": false,
    "feeds.section.topstories": true,
    "feeds.section.highlights": true
  },
  // Prefs listed as invalidating will prevent the prerendered version
  // of AS from being used if their value is something other than what is listed
  // here. This is required because some preferences cause the page layout to be
  // too different for the prerendered version to be used. Unfortunately, this
  // will result in users who have modified some of their preferences not being
  // able to get the benefits of prerendering.
  validation: ["showTopSites", "showSearch", "collapseTopSites", "section.highlights.collapsed", "section.topstories.collapsed",
  // This means if either of these are set to their default values,
  // prerendering can be used.
  { oneOf: ["feeds.section.topstories", "feeds.section.highlights"] }],
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
module.exports = {
  PrerenderData,
  _PrerenderData
};

/***/ }),
/* 31 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {/* eslint-env mozilla/frame-script */

const { createStore, combineReducers, applyMiddleware } = __webpack_require__(32);
const { actionTypes: at, actionCreators: ac, actionUtils: au } = __webpack_require__(0);

const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
const EARLY_QUEUED_ACTIONS = [at.SAVE_SESSION_PERF_DATA, at.PAGE_PRERENDERED];

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

const rehydrationMiddleware = store => next => action => {
  if (store._didRehydrate) {
    return next(action);
  }

  const isMergeStoreAction = action.type === MERGE_STORE_ACTION;
  const isRehydrationRequest = action.type === at.NEW_TAB_STATE_REQUEST;

  if (isRehydrationRequest) {
    store._didRequestInitialState = true;
    return next(action);
  }

  if (isMergeStoreAction) {
    store._didRehydrate = true;
    return next(action);
  }

  // If init happened after our request was made, we need to re-request
  if (store._didRequestInitialState && action.type === at.INIT) {
    return next(ac.SendToMain({ type: at.NEW_TAB_STATE_REQUEST }));
  }

  if (au.isBroadcastToContent(action) || au.isSendToContent(action)) {
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
  } else if (au.isFromMain(action)) {
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
module.exports = function initStore(reducers, initialState) {
  const store = createStore(mergeStateReducer(combineReducers(reducers)), initialState, global.addMessageListener && applyMiddleware(rehydrationMiddleware, queueEarlyMessageMiddleware, messageMiddleware));

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
};

module.exports.rehydrationMiddleware = rehydrationMiddleware;
module.exports.queueEarlyMessageMiddleware = queueEarlyMessageMiddleware;
module.exports.MERGE_STORE_ACTION = MERGE_STORE_ACTION;
module.exports.OUTGOING_MESSAGE_NAME = OUTGOING_MESSAGE_NAME;
module.exports.INCOMING_MESSAGE_NAME = INCOMING_MESSAGE_NAME;
module.exports.EARLY_QUEUED_ACTIONS = EARLY_QUEUED_ACTIONS;
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 32 */
/***/ (function(module, exports) {

module.exports = Redux;

/***/ }),
/* 33 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);
const { perfService: perfSvc } = __webpack_require__(11);

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

module.exports = class DetectUserSessionStart {
  constructor(store, options = {}) {
    this._store = store;
    // Overrides for testing
    this.document = options.document || global.document;
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

      this._store.dispatch(ac.SendToMain({
        type: at.SAVE_SESSION_PERF_DATA,
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
};
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ }),
/* 34 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {const DATABASE_NAME = "snippets_db";
const DATABASE_VERSION = 1;
const SNIPPETS_OBJECTSTORE_NAME = "snippets";
const SNIPPETS_UPDATE_INTERVAL_MS = 14400000; // 4 hours.

const SNIPPETS_ENABLED_EVENT = "Snippets:Enabled";
const SNIPPETS_DISABLED_EVENT = "Snippets:Disabled";

const { actionTypes: at, actionCreators: ac } = __webpack_require__(0);

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
    let blockList = this.blockList;
    if (!blockList.includes(id)) {
      blockList.push(id);
      this._dispatch(ac.SendToMain({ type: at.SNIPPETS_BLOCKLIST_UPDATED, data: blockList }));
      await this.set("blockList", blockList);
    }
  }

  disableOnboarding() {
    this._dispatch(ac.SendToMain({ type: at.DISABLE_ONBOARDING }));
  }

  showFirefoxAccounts() {
    this._dispatch(ac.SendToMain({ type: at.SHOW_FIREFOX_ACCOUNTS }));
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
    if (msg.data.type === at.SNIPPET_BLOCKED) {
      this.snippetsMap.set("blockList", msg.data.data);
      document.getElementById("snippets-container").style.display = "none";
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
      this.snippetsMap.set(`appData.${key}`, this.appData[key]);
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
    if (state.Prefs.values["feeds.snippets"] && !state.Prefs.values.disableSnippets && state.Snippets.initialized && !snippets.initialized &&
    // Don't call init multiple times
    !initializing) {
      initializing = true;
      await snippets.init({ appData: state.Snippets });
      initializing = false;
    } else if ((state.Prefs.values["feeds.snippets"] === false || state.Prefs.values.disableSnippets === true) && snippets.initialized) {
      snippets.uninit();
    }
  });

  // These values are returned for testing purposes
  return snippets;
}

module.exports = {
  addSnippetsSubscriber,
  SnippetsMap,
  SnippetsProvider,
  SNIPPETS_UPDATE_INTERVAL_MS
};
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(4)))

/***/ })
/******/ ]);