/*! THIS FILE IS AUTO-GENERATED: webpack.system-addon.config.js */
var NewtabRenderUtils;
/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	// The require scope
/******/ 	var __webpack_require__ = {};
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/compat get default export */
/******/ 	(() => {
/******/ 		// getDefaultExport function for compatibility with non-harmony modules
/******/ 		__webpack_require__.n = (module) => {
/******/ 			var getter = module && module.__esModule ?
/******/ 				() => (module['default']) :
/******/ 				() => (module);
/******/ 			__webpack_require__.d(getter, { a: getter });
/******/ 			return getter;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/global */
/******/ 	(() => {
/******/ 		__webpack_require__.g = (function() {
/******/ 			if (typeof globalThis === 'object') return globalThis;
/******/ 			try {
/******/ 				return this || new Function('return this')();
/******/ 			} catch (e) {
/******/ 				if (typeof window === 'object') return window;
/******/ 			}
/******/ 		})();
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
var __webpack_exports__ = {};
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "NewTab": () => (/* binding */ NewTab),
  "renderCache": () => (/* binding */ renderCache),
  "renderWithoutState": () => (/* binding */ renderWithoutState)
});

;// CONCATENATED MODULE: ./common/Actions.sys.mjs
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const MAIN_MESSAGE_TYPE = "ActivityStream:Main";
const CONTENT_MESSAGE_TYPE = "ActivityStream:Content";
const PRELOAD_MESSAGE_TYPE = "ActivityStream:PreloadedBrowser";
const UI_CODE = 1;
const BACKGROUND_PROCESS = 2;

/**
 * globalImportContext - Are we in UI code (i.e. react, a dom) or some kind of background process?
 *                       Use this in action creators if you need different logic
 *                       for ui/background processes.
 */
const globalImportContext =
  typeof Window === "undefined" ? BACKGROUND_PROCESS : UI_CODE;

// Create an object that avoids accidental differing key/value pairs:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
const actionTypes = {};

for (const type of [
  "ABOUT_SPONSORED_TOP_SITES",
  "ADDONS_INFO_REQUEST",
  "ADDONS_INFO_RESPONSE",
  "ARCHIVE_FROM_POCKET",
  "AS_ROUTER_INITIALIZED",
  "AS_ROUTER_PREF_CHANGED",
  "AS_ROUTER_TARGETING_UPDATE",
  "AS_ROUTER_TELEMETRY_USER_EVENT",
  "BLOCK_URL",
  "BOOKMARK_URL",
  "CLEAR_PREF",
  "COPY_DOWNLOAD_LINK",
  "DELETE_BOOKMARK_BY_ID",
  "DELETE_FROM_POCKET",
  "DELETE_HISTORY_URL",
  "DIALOG_CANCEL",
  "DIALOG_OPEN",
  "DISABLE_SEARCH",
  "DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE",
  "DISCOVERY_STREAM_CONFIG_CHANGE",
  "DISCOVERY_STREAM_CONFIG_RESET",
  "DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS",
  "DISCOVERY_STREAM_CONFIG_SETUP",
  "DISCOVERY_STREAM_CONFIG_SET_VALUE",
  "DISCOVERY_STREAM_DEV_EXPIRE_CACHE",
  "DISCOVERY_STREAM_DEV_IDLE_DAILY",
  "DISCOVERY_STREAM_DEV_SYNC_RS",
  "DISCOVERY_STREAM_DEV_SYSTEM_TICK",
  "DISCOVERY_STREAM_EXPERIMENT_DATA",
  "DISCOVERY_STREAM_FEEDS_UPDATE",
  "DISCOVERY_STREAM_FEED_UPDATE",
  "DISCOVERY_STREAM_IMPRESSION_STATS",
  "DISCOVERY_STREAM_LAYOUT_RESET",
  "DISCOVERY_STREAM_LAYOUT_UPDATE",
  "DISCOVERY_STREAM_LINK_BLOCKED",
  "DISCOVERY_STREAM_LOADED_CONTENT",
  "DISCOVERY_STREAM_PERSONALIZATION_INIT",
  "DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED",
  "DISCOVERY_STREAM_PERSONALIZATION_OVERRIDE",
  "DISCOVERY_STREAM_PERSONALIZATION_RESET",
  "DISCOVERY_STREAM_PERSONALIZATION_TOGGLE",
  "DISCOVERY_STREAM_PERSONALIZATION_UPDATED",
  "DISCOVERY_STREAM_POCKET_STATE_INIT",
  "DISCOVERY_STREAM_POCKET_STATE_SET",
  "DISCOVERY_STREAM_PREFS_SETUP",
  "DISCOVERY_STREAM_RECENT_SAVES",
  "DISCOVERY_STREAM_RETRY_FEED",
  "DISCOVERY_STREAM_SPOCS_CAPS",
  "DISCOVERY_STREAM_SPOCS_ENDPOINT",
  "DISCOVERY_STREAM_SPOCS_PLACEMENTS",
  "DISCOVERY_STREAM_SPOCS_UPDATE",
  "DISCOVERY_STREAM_SPOC_BLOCKED",
  "DISCOVERY_STREAM_SPOC_IMPRESSION",
  "DISCOVERY_STREAM_USER_EVENT",
  "DOWNLOAD_CHANGED",
  "FAKE_FOCUS_SEARCH",
  "FILL_SEARCH_TERM",
  "HANDOFF_SEARCH_TO_AWESOMEBAR",
  "HIDE_PERSONALIZE",
  "HIDE_PRIVACY_INFO",
  "INIT",
  "NEW_TAB_INIT",
  "NEW_TAB_INITIAL_STATE",
  "NEW_TAB_LOAD",
  "NEW_TAB_REHYDRATED",
  "NEW_TAB_STATE_REQUEST",
  "NEW_TAB_UNLOAD",
  "OPEN_DOWNLOAD_FILE",
  "OPEN_LINK",
  "OPEN_NEW_WINDOW",
  "OPEN_PRIVATE_WINDOW",
  "OPEN_WEBEXT_SETTINGS",
  "PARTNER_LINK_ATTRIBUTION",
  "PLACES_BOOKMARKS_REMOVED",
  "PLACES_BOOKMARK_ADDED",
  "PLACES_HISTORY_CLEARED",
  "PLACES_LINKS_CHANGED",
  "PLACES_LINKS_DELETED",
  "PLACES_LINK_BLOCKED",
  "PLACES_SAVED_TO_POCKET",
  "POCKET_CTA",
  "POCKET_LINK_DELETED_OR_ARCHIVED",
  "POCKET_LOGGED_IN",
  "POCKET_WAITING_FOR_SPOC",
  "PREFS_INITIAL_VALUES",
  "PREF_CHANGED",
  "PREVIEW_REQUEST",
  "PREVIEW_REQUEST_CANCEL",
  "PREVIEW_RESPONSE",
  "REMOVE_DOWNLOAD_FILE",
  "RICH_ICON_MISSING",
  "SAVE_SESSION_PERF_DATA",
  "SAVE_TO_POCKET",
  "SCREENSHOT_UPDATED",
  "SECTION_DEREGISTER",
  "SECTION_DISABLE",
  "SECTION_ENABLE",
  "SECTION_MOVE",
  "SECTION_OPTIONS_CHANGED",
  "SECTION_REGISTER",
  "SECTION_UPDATE",
  "SECTION_UPDATE_CARD",
  "SETTINGS_CLOSE",
  "SETTINGS_OPEN",
  "SET_PREF",
  "SHOW_DOWNLOAD_FILE",
  "SHOW_FIREFOX_ACCOUNTS",
  "SHOW_PERSONALIZE",
  "SHOW_PRIVACY_INFO",
  "SHOW_SEARCH",
  "SKIPPED_SIGNIN",
  "SNIPPETS_BLOCKLIST_CLEARED",
  "SNIPPETS_BLOCKLIST_UPDATED",
  "SNIPPETS_DATA",
  "SNIPPETS_PREVIEW_MODE",
  "SNIPPETS_RESET",
  "SNIPPET_BLOCKED",
  "SOV_UPDATED",
  "SUBMIT_EMAIL",
  "SUBMIT_SIGNIN",
  "SYSTEM_TICK",
  "TELEMETRY_IMPRESSION_STATS",
  "TELEMETRY_USER_EVENT",
  "TOP_SITES_CANCEL_EDIT",
  "TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL",
  "TOP_SITES_EDIT",
  "TOP_SITES_INSERT",
  "TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL",
  "TOP_SITES_ORGANIC_IMPRESSION_STATS",
  "TOP_SITES_PIN",
  "TOP_SITES_PREFS_UPDATED",
  "TOP_SITES_SPONSORED_IMPRESSION_STATS",
  "TOP_SITES_UNPIN",
  "TOP_SITES_UPDATED",
  "TOTAL_BOOKMARKS_REQUEST",
  "TOTAL_BOOKMARKS_RESPONSE",
  "UNINIT",
  "UPDATE_PINNED_SEARCH_SHORTCUTS",
  "UPDATE_SEARCH_SHORTCUTS",
  "UPDATE_SECTION_PREFS",
  "WEBEXT_CLICK",
  "WEBEXT_DISMISS",
]) {
  actionTypes[type] = type;
}

// Helper function for creating routed actions between content and main
// Not intended to be used by consumers
function _RouteMessage(action, options) {
  const meta = action.meta ? { ...action.meta } : {};
  if (!options || !options.from || !options.to) {
    throw new Error(
      "Routed Messages must have options as the second parameter, and must at least include a .from and .to property."
    );
  }
  // For each of these fields, if they are passed as an option,
  // add them to the action. If they are not defined, remove them.
  ["from", "to", "toTarget", "fromTarget", "skipMain", "skipLocal"].forEach(
    o => {
      if (typeof options[o] !== "undefined") {
        meta[o] = options[o];
      } else if (meta[o]) {
        delete meta[o];
      }
    }
  );
  return { ...action, meta };
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
    skipLocal,
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
    to: CONTENT_MESSAGE_TYPE,
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
    throw new Error(
      "You must provide a target ID as the second parameter of AlsoToOneContent. If you want to send to all content processes, use BroadcastToContent"
    );
  }
  return _RouteMessage(action, {
    from: MAIN_MESSAGE_TYPE,
    to: CONTENT_MESSAGE_TYPE,
    toTarget: target,
    skipMain,
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
    to: PRELOAD_MESSAGE_TYPE,
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
    data,
  });
}

/**
 * DiscoveryStreamUserEvent - A telemetry ping indicating a user action from Discovery Stream. This should only
 *                     be sent from the UI during a user session.
 *
 * @param  {object} data Fields to include in the ping (source, etc.)
 * @return {object} An AlsoToMain action
 */
function DiscoveryStreamUserEvent(data) {
  return AlsoToMain({
    type: actionTypes.DISCOVERY_STREAM_USER_EVENT,
    data,
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
    data,
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
    data,
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
function DiscoveryStreamImpressionStats(
  data,
  importContext = globalImportContext
) {
  const action = {
    type: actionTypes.DISCOVERY_STREAM_IMPRESSION_STATS,
    data,
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
function DiscoveryStreamLoadedContent(
  data,
  importContext = globalImportContext
) {
  const action = {
    type: actionTypes.DISCOVERY_STREAM_LOADED_CONTENT,
    data,
  };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function SetPref(name, value, importContext = globalImportContext) {
  const action = { type: actionTypes.SET_PREF, data: { name, value } };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

function WebExtEvent(type, data, importContext = globalImportContext) {
  if (!data || !data.source) {
    throw new Error(
      'WebExtEvent actions should include a property "source", the id of the webextension that should receive the event.'
    );
  }
  const action = { type, data };
  return importContext === UI_CODE ? AlsoToMain(action) : action;
}

const actionCreators = {
  BroadcastToContent,
  UserEvent,
  DiscoveryStreamUserEvent,
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
  DiscoveryStreamLoadedContent,
};

// These are helpers to test for certain kinds of actions
const actionUtils = {
  isSendToMain(action) {
    if (!action.meta) {
      return false;
    }
    return (
      action.meta.to === MAIN_MESSAGE_TYPE &&
      action.meta.from === CONTENT_MESSAGE_TYPE
    );
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
    return (
      action.meta.to === PRELOAD_MESSAGE_TYPE &&
      action.meta.from === MAIN_MESSAGE_TYPE
    );
  },
  isFromMain(action) {
    if (!action.meta) {
      return false;
    }
    return (
      action.meta.from === MAIN_MESSAGE_TYPE &&
      action.meta.to === CONTENT_MESSAGE_TYPE
    );
  },
  getPortIdOfSender(action) {
    return (action.meta && action.meta.fromTarget) || null;
  },
  _RouteMessage,
};

;// CONCATENATED MODULE: ./common/ActorConstants.sys.mjs
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const MESSAGE_TYPE_LIST = [
  "BLOCK_MESSAGE_BY_ID",
  "USER_ACTION",
  "IMPRESSION",
  "TRIGGER",
  "NEWTAB_MESSAGE_REQUEST",
  // PB is Private Browsing
  "PBNEWTAB_MESSAGE_REQUEST",
  "DOORHANGER_TELEMETRY",
  "TOOLBAR_BADGE_TELEMETRY",
  "TOOLBAR_PANEL_TELEMETRY",
  "MOMENTS_PAGE_TELEMETRY",
  "INFOBAR_TELEMETRY",
  "SPOTLIGHT_TELEMETRY",
  "TOAST_NOTIFICATION_TELEMETRY",
  "AS_ROUTER_TELEMETRY_USER_EVENT",

  // Admin types
  "ADMIN_CONNECT_STATE",
  "UNBLOCK_MESSAGE_BY_ID",
  "UNBLOCK_ALL",
  "BLOCK_BUNDLE",
  "UNBLOCK_BUNDLE",
  "DISABLE_PROVIDER",
  "ENABLE_PROVIDER",
  "EVALUATE_JEXL_EXPRESSION",
  "EXPIRE_QUERY_CACHE",
  "FORCE_ATTRIBUTION",
  "FORCE_WHATSNEW_PANEL",
  "FORCE_PRIVATE_BROWSING_WINDOW",
  "CLOSE_WHATSNEW_PANEL",
  "OVERRIDE_MESSAGE",
  "MODIFY_MESSAGE_JSON",
  "RESET_PROVIDER_PREF",
  "SET_PROVIDER_USER_PREF",
  "RESET_GROUPS_STATE",
  "RESET_MESSAGE_STATE",
  "RESET_SCREEN_IMPRESSIONS",
  "EDIT_STATE",
];

const MESSAGE_TYPE_HASH = MESSAGE_TYPE_LIST.reduce((hash, value) => {
  hash[value] = value;
  return hash;
}, {});

;// CONCATENATED MODULE: ./content-src/asrouter/asrouter-utils.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const ASRouterUtils = {
  addListener(listener) {
    if (__webpack_require__.g.ASRouterAddParentListener) {
      __webpack_require__.g.ASRouterAddParentListener(listener);
    }
  },

  removeListener(listener) {
    if (__webpack_require__.g.ASRouterRemoveParentListener) {
      __webpack_require__.g.ASRouterRemoveParentListener(listener);
    }
  },

  sendMessage(action) {
    if (__webpack_require__.g.ASRouterMessage) {
      return __webpack_require__.g.ASRouterMessage(action);
    }

    throw new Error(`Unexpected call:\n${JSON.stringify(action, null, 3)}`);
  },

  blockById(id, options) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.BLOCK_MESSAGE_BY_ID,
      data: {
        id,
        ...options
      }
    });
  },

  modifyMessageJson(content) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.MODIFY_MESSAGE_JSON,
      data: {
        content
      }
    });
  },

  executeAction(button_action) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.USER_ACTION,
      data: button_action
    });
  },

  unblockById(id) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.UNBLOCK_MESSAGE_BY_ID,
      data: {
        id
      }
    });
  },

  blockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.BLOCK_BUNDLE,
      data: {
        bundle
      }
    });
  },

  unblockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.UNBLOCK_BUNDLE,
      data: {
        bundle
      }
    });
  },

  overrideMessage(id) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.OVERRIDE_MESSAGE,
      data: {
        id
      }
    });
  },

  editState(key, value) {
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.EDIT_STATE,
      data: {
        [key]: value
      }
    });
  },

  sendTelemetry(ping) {
    return ASRouterUtils.sendMessage(actionCreators.ASRouterUserEvent(ping));
  },

  getPreviewEndpoint() {
    if (__webpack_require__.g.document && __webpack_require__.g.document.location && __webpack_require__.g.document.location.href.includes("endpoint")) {
      const params = new URLSearchParams(__webpack_require__.g.document.location.href.slice(__webpack_require__.g.document.location.href.indexOf("endpoint")));

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
    return new URLSearchParams(__webpack_require__.g.document.location.href.slice(__webpack_require__.g.document.location.href.indexOf("theme"))).get("theme");
  },

  getPreviewDir() {
    return new URLSearchParams(__webpack_require__.g.document.location.href.slice(__webpack_require__.g.document.location.href.indexOf("dir"))).get("dir");
  }

};
;// CONCATENATED MODULE: external "ReactRedux"
const external_ReactRedux_namespaceObject = ReactRedux;
;// CONCATENATED MODULE: external "React"
const external_React_namespaceObject = React;
var external_React_default = /*#__PURE__*/__webpack_require__.n(external_React_namespaceObject);
;// CONCATENATED MODULE: ./content-src/components/ASRouterAdmin/SimpleHashRouter.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SimpleHashRouter extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onHashChange = this.onHashChange.bind(this);
    this.state = {
      hash: __webpack_require__.g.location.hash
    };
  }

  onHashChange() {
    this.setState({
      hash: __webpack_require__.g.location.hash
    });
  }

  componentWillMount() {
    __webpack_require__.g.addEventListener("hashchange", this.onHashChange);
  }

  componentWillUnmount() {
    __webpack_require__.g.removeEventListener("hashchange", this.onHashChange);
  }

  render() {
    const [, ...routes] = this.state.hash.split("-");
    return /*#__PURE__*/external_React_default().cloneElement(this.props.children, {
      location: {
        hash: this.state.hash,
        routes
      }
    });
  }

}
;// CONCATENATED MODULE: ./content-src/components/ASRouterAdmin/CopyButton.jsx
function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const CopyButton = ({
  className,
  label,
  copiedLabel,
  inputSelector,
  transformer,
  ...props
}) => {
  const [copied, setCopied] = (0,external_React_namespaceObject.useState)(false);
  const timeout = (0,external_React_namespaceObject.useRef)(null);
  const onClick = (0,external_React_namespaceObject.useCallback)(() => {
    let text = document.querySelector(inputSelector).value;

    if (transformer) {
      text = transformer(text);
    }

    navigator.clipboard.writeText(text);
    clearTimeout(timeout.current);
    setCopied(true);
    timeout.current = setTimeout(() => setCopied(false), 1500);
  }, [inputSelector, transformer]);
  return /*#__PURE__*/external_React_default().createElement("button", _extends({
    className: className,
    onClick: e => onClick()
  }, props), copied && copiedLabel || label);
};
;// CONCATENATED MODULE: ./content-src/components/ASRouterAdmin/ImpressionsSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



const stringify = json => JSON.stringify(json, null, 2);

const ImpressionsSection = ({
  messageImpressions,
  groupImpressions,
  screenImpressions
}) => {
  const handleSaveMessageImpressions = (0,external_React_namespaceObject.useCallback)(newImpressions => {
    ASRouterUtils.editState("messageImpressions", newImpressions);
  }, []);
  const handleSaveGroupImpressions = (0,external_React_namespaceObject.useCallback)(newImpressions => {
    ASRouterUtils.editState("groupImpressions", newImpressions);
  }, []);
  const handleSaveScreenImpressions = (0,external_React_namespaceObject.useCallback)(newImpressions => {
    ASRouterUtils.editState("screenImpressions", newImpressions);
  }, []);
  const handleResetMessageImpressions = (0,external_React_namespaceObject.useCallback)(() => {
    ASRouterUtils.sendMessage({
      type: "RESET_MESSAGE_STATE"
    });
  }, []);
  const handleResetGroupImpressions = (0,external_React_namespaceObject.useCallback)(() => {
    ASRouterUtils.sendMessage({
      type: "RESET_GROUPS_STATE"
    });
  }, []);
  const handleResetScreenImpressions = (0,external_React_namespaceObject.useCallback)(() => {
    ASRouterUtils.sendMessage({
      type: "RESET_SCREEN_IMPRESSIONS"
    });
  }, []);
  return /*#__PURE__*/external_React_default().createElement("div", {
    className: "impressions-section"
  }, /*#__PURE__*/external_React_default().createElement(ImpressionsItem, {
    impressions: messageImpressions,
    label: "Message Impressions",
    description: "Message impressions are stored in an object, where each key is a message ID and each value is an array of timestamps. They are cleaned up when a message with that ID stops existing in ASRouter state (such as at the end of an experiment).",
    onSave: handleSaveMessageImpressions,
    onReset: handleResetMessageImpressions
  }), /*#__PURE__*/external_React_default().createElement(ImpressionsItem, {
    impressions: groupImpressions,
    label: "Group Impressions",
    description: "Group impressions are stored in an object, where each key is a group ID and each value is an array of timestamps. They are never cleaned up.",
    onSave: handleSaveGroupImpressions,
    onReset: handleResetGroupImpressions
  }), /*#__PURE__*/external_React_default().createElement(ImpressionsItem, {
    impressions: screenImpressions,
    label: "Screen Impressions",
    description: "Screen impressions are stored in an object, where each key is a screen ID and each value is the most recent timestamp that screen was shown. They are never cleaned up.",
    onSave: handleSaveScreenImpressions,
    onReset: handleResetScreenImpressions
  }));
};

const ImpressionsItem = ({
  impressions,
  label,
  description,
  validator,
  onSave,
  onReset
}) => {
  const [json, setJson] = (0,external_React_namespaceObject.useState)(stringify(impressions));
  const modified = (0,external_React_namespaceObject.useRef)(false);
  const isValidJson = (0,external_React_namespaceObject.useCallback)(text => {
    try {
      JSON.parse(text);
      return validator ? validator(text) : true;
    } catch (e) {
      return false;
    }
  }, [validator]);
  const jsonIsInvalid = (0,external_React_namespaceObject.useMemo)(() => !isValidJson(json), [json, isValidJson]);
  const handleChange = (0,external_React_namespaceObject.useCallback)(e => {
    setJson(e.target.value);
    modified.current = true;
  }, []);
  const handleSave = (0,external_React_namespaceObject.useCallback)(() => {
    if (jsonIsInvalid) {
      return;
    }

    const newImpressions = JSON.parse(json);
    modified.current = false;
    onSave(newImpressions);
  }, [json, jsonIsInvalid, onSave]);
  const handleReset = (0,external_React_namespaceObject.useCallback)(() => {
    modified.current = false;
    onReset();
  }, [onReset]);
  (0,external_React_namespaceObject.useEffect)(() => {
    if (!modified.current) {
      setJson(stringify(impressions));
    }
  }, [impressions]);
  return /*#__PURE__*/external_React_default().createElement("div", {
    className: "impressions-item"
  }, /*#__PURE__*/external_React_default().createElement("span", {
    className: "impressions-category"
  }, label), description ? /*#__PURE__*/external_React_default().createElement("p", {
    className: "impressions-description"
  }, description) : null, /*#__PURE__*/external_React_default().createElement("div", {
    className: "impressions-inner-box"
  }, /*#__PURE__*/external_React_default().createElement("div", {
    className: "impressions-buttons"
  }, /*#__PURE__*/external_React_default().createElement("button", {
    className: "button primary",
    disabled: jsonIsInvalid,
    onClick: handleSave
  }, "Save"), /*#__PURE__*/external_React_default().createElement("button", {
    className: "button reset",
    onClick: handleReset
  }, "Reset")), /*#__PURE__*/external_React_default().createElement("div", {
    className: "impressions-editor"
  }, /*#__PURE__*/external_React_default().createElement("textarea", {
    className: "general-textarea",
    value: json,
    onChange: handleChange
  }))));
};
;// CONCATENATED MODULE: ./content-src/components/ASRouterAdmin/ASRouterAdmin.jsx
function ASRouterAdmin_extends() { ASRouterAdmin_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return ASRouterAdmin_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */








const Row = props => /*#__PURE__*/external_React_default().createElement("tr", ASRouterAdmin_extends({
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

class ToggleStoryButton extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.onClick(this.props.story);
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("button", {
      onClick: this.handleClick
    }, "collapse/open");
  }

}
class ToggleMessageJSON extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.toggleJSON(this.props.msgId);
  }

  render() {
    let iconName = this.props.isCollapsed ? "icon icon-arrowhead-forward-small" : "icon icon-arrowhead-down-small";
    return /*#__PURE__*/external_React_default().createElement("button", {
      className: "clearButton",
      onClick: this.handleClick
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: iconName
    }));
  }

}
class TogglePrefCheckbox extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onChange = this.onChange.bind(this);
  }

  onChange(event) {
    this.props.onChange(this.props.pref, event.target.checked);
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("input", {
      type: "checkbox",
      checked: this.props.checked,
      onChange: this.onChange,
      disabled: this.props.disabled
    }), " ", this.props.pref, " ");
  }

}
class Personalization extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.togglePersonalization = this.togglePersonalization.bind(this);
  }

  togglePersonalization() {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.DISCOVERY_STREAM_PERSONALIZATION_TOGGLE
    }));
  }

  render() {
    const {
      lastUpdated,
      initialized
    } = this.props.state.Personalization;
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      colSpan: "2"
    }, /*#__PURE__*/external_React_default().createElement(TogglePrefCheckbox, {
      checked: this.props.personalized,
      pref: "personalized",
      onChange: this.togglePersonalization
    }))), /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Personalization Last Updated"), /*#__PURE__*/external_React_default().createElement("td", null, relativeTime(lastUpdated) || "(no data)")), /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Personalization Initialized"), /*#__PURE__*/external_React_default().createElement("td", null, initialized ? "true" : "false")))));
  }

}
class DiscoveryStreamAdmin extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.restorePrefDefaults = this.restorePrefDefaults.bind(this);
    this.setConfigValue = this.setConfigValue.bind(this);
    this.expireCache = this.expireCache.bind(this);
    this.refreshCache = this.refreshCache.bind(this);
    this.idleDaily = this.idleDaily.bind(this);
    this.systemTick = this.systemTick.bind(this);
    this.syncRemoteSettings = this.syncRemoteSettings.bind(this);
    this.onStoryToggle = this.onStoryToggle.bind(this);
    this.state = {
      toggledStories: {}
    };
  }

  setConfigValue(name, value) {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.DISCOVERY_STREAM_CONFIG_SET_VALUE,
      data: {
        name,
        value
      }
    }));
  }

  restorePrefDefaults(event) {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS
    }));
  }

  refreshCache() {
    const {
      config
    } = this.props.state.DiscoveryStream;
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.DISCOVERY_STREAM_CONFIG_CHANGE,
      data: config
    }));
  }

  dispatchSimpleAction(type) {
    this.props.dispatch(actionCreators.OnlyToMain({
      type
    }));
  }

  systemTick() {
    this.dispatchSimpleAction(actionTypes.DISCOVERY_STREAM_DEV_SYSTEM_TICK);
  }

  expireCache() {
    this.dispatchSimpleAction(actionTypes.DISCOVERY_STREAM_DEV_EXPIRE_CACHE);
  }

  idleDaily() {
    this.dispatchSimpleAction(actionTypes.DISCOVERY_STREAM_DEV_IDLE_DAILY);
  }

  syncRemoteSettings() {
    this.dispatchSimpleAction(actionTypes.DISCOVERY_STREAM_DEV_SYNC_RS);
  }

  renderComponent(width, component) {
    return /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Type"), /*#__PURE__*/external_React_default().createElement("td", null, component.type)), /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Width"), /*#__PURE__*/external_React_default().createElement("td", null, width)), component.feed && this.renderFeed(component.feed)));
  }

  renderFeedData(url) {
    var _feed$recommendations;

    const {
      feeds
    } = this.props.state.DiscoveryStream;
    const feed = feeds.data[url].data;
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h4", null, "Feed url: ", url), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, (_feed$recommendations = feed.recommendations) === null || _feed$recommendations === void 0 ? void 0 : _feed$recommendations.map(story => this.renderStoryData(story)))));
  }

  renderFeedsData() {
    const {
      feeds
    } = this.props.state.DiscoveryStream;
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, Object.keys(feeds.data).map(url => this.renderFeedData(url)));
  }

  renderSpocs() {
    const {
      spocs
    } = this.props.state.DiscoveryStream;
    let spocsData = [];

    if (spocs.data && spocs.data.spocs && spocs.data.spocs.items) {
      spocsData = spocs.data.spocs.items || [];
    }

    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "spocs_endpoint"), /*#__PURE__*/external_React_default().createElement("td", null, spocs.spocs_endpoint)), /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Data last fetched"), /*#__PURE__*/external_React_default().createElement("td", null, relativeTime(spocs.lastUpdated))))), /*#__PURE__*/external_React_default().createElement("h4", null, "Spoc data"), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, spocsData.map(spoc => this.renderStoryData(spoc)))), /*#__PURE__*/external_React_default().createElement("h4", null, "Spoc frequency caps"), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, spocs.frequency_caps.map(spoc => this.renderStoryData(spoc)))));
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

    return /*#__PURE__*/external_React_default().createElement("tr", {
      className: "message-item",
      key: story.id
    }, /*#__PURE__*/external_React_default().createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/external_React_default().createElement("span", null, story.id, " ", /*#__PURE__*/external_React_default().createElement("br", null)), /*#__PURE__*/external_React_default().createElement(ToggleStoryButton, {
      story: story,
      onClick: this.onStoryToggle
    })), /*#__PURE__*/external_React_default().createElement("td", {
      className: "message-summary"
    }, /*#__PURE__*/external_React_default().createElement("pre", null, storyData)));
  }

  renderFeed(feed) {
    const {
      feeds
    } = this.props.state.DiscoveryStream;

    if (!feed.url) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Feed url"), /*#__PURE__*/external_React_default().createElement("td", null, feed.url)), /*#__PURE__*/external_React_default().createElement(Row, null, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Data last fetched"), /*#__PURE__*/external_React_default().createElement("td", null, relativeTime(feeds.data[feed.url] ? feeds.data[feed.url].lastUpdated : null) || "(no data)")));
  }

  render() {
    const prefToggles = "enabled collapsible".split(" ");
    const {
      config,
      layout
    } = this.props.state.DiscoveryStream;
    const personalized = this.props.otherPrefs["discoverystream.personalization.enabled"];
    return /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.restorePrefDefaults
    }, "Restore Pref Defaults"), " ", /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.refreshCache
    }, "Refresh Cache"), /*#__PURE__*/external_React_default().createElement("br", null), /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.expireCache
    }, "Expire Cache"), " ", /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.systemTick
    }, "Trigger System Tick"), " ", /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.idleDaily
    }, "Trigger Idle Daily"), /*#__PURE__*/external_React_default().createElement("br", null), /*#__PURE__*/external_React_default().createElement("button", {
      className: "button",
      onClick: this.syncRemoteSettings
    }, "Sync Remote Settings"), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, prefToggles.map(pref => /*#__PURE__*/external_React_default().createElement(Row, {
      key: pref
    }, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement(TogglePrefCheckbox, {
      checked: config[pref],
      pref: pref,
      onChange: this.setConfigValue
    })))))), /*#__PURE__*/external_React_default().createElement("h3", null, "Layout"), layout.map((row, rowIndex) => /*#__PURE__*/external_React_default().createElement("div", {
      key: `row-${rowIndex}`
    }, row.components.map((component, componentIndex) => /*#__PURE__*/external_React_default().createElement("div", {
      key: `component-${componentIndex}`,
      className: "ds-component"
    }, this.renderComponent(row.width, component))))), /*#__PURE__*/external_React_default().createElement("h3", null, "Personalization"), /*#__PURE__*/external_React_default().createElement(Personalization, {
      personalized: personalized,
      dispatch: this.props.dispatch,
      state: {
        Personalization: this.props.state.Personalization
      }
    }), /*#__PURE__*/external_React_default().createElement("h3", null, "Spocs"), this.renderSpocs(), /*#__PURE__*/external_React_default().createElement("h3", null, "Feeds Data"), this.renderFeedsData());
  }

}
class ASRouterAdminInner extends (external_React_default()).PureComponent {
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
    this.handleOpenPB = this.handleOpenPB.bind(this);
    this.selectPBMessage = this.selectPBMessage.bind(this);
    this.resetPBJSON = this.resetPBJSON.bind(this);
    this.resetPBMessageState = this.resetPBMessageState.bind(this);
    this.toggleJSON = this.toggleJSON.bind(this);
    this.toggleAllMessages = this.toggleAllMessages.bind(this);
    this.resetGroups = this.resetGroups.bind(this);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.setStateFromParent = this.setStateFromParent.bind(this);
    this.setState = this.setState.bind(this);
    this.state = {
      messageFilter: "all",
      messageGroupsFilter: "all",
      collapsedMessages: [],
      modifiedMessages: [],
      selectedPBMessage: "",
      evaluationStatus: {},
      stringTargetingParameters: null,
      newStringTargetingParameters: null,
      copiedToClipboard: false,
      attributionParameters: {
        source: "addons.mozilla.org",
        medium: "referral",
        campaign: "non-fx-button",
        content: `rta:${btoa("uBlock0@raymondhill.net")}`,
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
    ASRouterUtils.addListener(this.onMessageFromParent);
    const endpoint = ASRouterUtils.getPreviewEndpoint();
    ASRouterUtils.sendMessage({
      type: "ADMIN_CONNECT_STATE",
      data: {
        endpoint
      }
    }).then(this.setStateFromParent);
  }

  handleBlock(msg) {
    return () => ASRouterUtils.blockById(msg.id);
  }

  handleUnblock(msg) {
    return () => ASRouterUtils.unblockById(msg.id);
  }

  resetJSON(msg) {
    // reset the displayed JSON for the given message
    document.getElementById(`${msg.id}-textarea`).value = JSON.stringify(msg, null, 2); // remove the message from the list of modified IDs

    let index = this.state.modifiedMessages.indexOf(msg.id);
    this.setState(prevState => ({
      modifiedMessages: [...prevState.modifiedMessages.slice(0, index), ...prevState.modifiedMessages.slice(index + 1)]
    }));
  }

  handleOverride(id) {
    return () => ASRouterUtils.overrideMessage(id).then(state => {
      this.setStateFromParent(state);
      this.props.notifyContent({
        message: state.message
      });
    });
  }

  resetPBMessageState() {
    // Iterate over Private Browsing messages and block/unblock each one to clear impressions
    const PBMessages = this.state.messages.filter(message => message.template === "pb_newtab"); // messages from state go here

    PBMessages.forEach(message => {
      if (message !== null && message !== void 0 && message.id) {
        ASRouterUtils.blockById(message.id);
        ASRouterUtils.unblockById(message.id);
      }
    }); // Clear the selected messages & radio buttons

    document.getElementById("clear radio").checked = true;
    this.selectPBMessage("clear");
  }

  resetPBJSON(msg) {
    // reset the displayed JSON for the given message
    document.getElementById(`${msg.id}-textarea`).value = JSON.stringify(msg, null, 2);
  }

  handleOpenPB() {
    ASRouterUtils.sendMessage({
      type: "FORCE_PRIVATE_BROWSING_WINDOW",
      data: {
        message: {
          content: this.state.selectedPBMessage
        }
      }
    });
  }

  expireCache() {
    ASRouterUtils.sendMessage({
      type: "EXPIRE_QUERY_CACHE"
    });
  }

  resetPref() {
    ASRouterUtils.sendMessage({
      type: "RESET_PROVIDER_PREF"
    });
  }

  resetGroups(id, value) {
    ASRouterUtils.sendMessage({
      type: "RESET_GROUPS_STATE"
    }).then(this.setStateFromParent);
  }

  handleExpressionEval() {
    const context = {};

    for (const param of Object.keys(this.state.stringTargetingParameters)) {
      const value = this.state.stringTargetingParameters[param];
      context[param] = value ? JSON.parse(value) : null;
    }

    ASRouterUtils.sendMessage({
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
        console.error(`Error parsing value of parameter ${name}`);
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
    return ASRouterUtils.sendMessage({
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
    ASRouterUtils.sendMessage({
      type: "DISABLE_PROVIDER",
      data: providerId
    });

    if (!isUserEnabled) {
      ASRouterUtils.sendMessage({
        type: "SET_PROVIDER_USER_PREF",
        data: {
          id: providerId,
          value: true
        }
      });
    }

    ASRouterUtils.sendMessage({
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
        ASRouterUtils.sendMessage({
          type: "SET_PROVIDER_USER_PREF",
          data: {
            id: provider.id,
            value: true
          }
        });
      }

      if (!isSystemEnabled) {
        ASRouterUtils.sendMessage({
          type: "ENABLE_PROVIDER",
          data: provider.id
        });
      }
    } else {
      ASRouterUtils.sendMessage({
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
    ASRouterUtils.sendMessage(action);
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
    const aboutMessagePreviewSupported = ["infobar", "spotlight", "cfr_doorhanger"].includes(msg.template);
    let itemClassName = "message-item";

    if (isBlocked) {
      itemClassName += " blocked";
    }

    return /*#__PURE__*/external_React_default().createElement("tr", {
      className: itemClassName,
      key: `${msg.id}-${msg.provider}`
    }, /*#__PURE__*/external_React_default().createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/external_React_default().createElement("span", null, msg.id, " ", /*#__PURE__*/external_React_default().createElement("br", null))), /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement(ToggleMessageJSON, {
      msgId: `${msg.id}`,
      toggleJSON: this.toggleJSON,
      isCollapsed: isCollapsed
    })), /*#__PURE__*/external_React_default().createElement("td", {
      className: "button-column"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: `button ${isBlocked ? "" : " primary"}`,
      onClick: isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)
    }, isBlocked ? "Unblock" : "Block"), // eslint-disable-next-line no-nested-ternary
    isBlocked ? null : isModified ? /*#__PURE__*/external_React_default().createElement("button", {
      className: "button restore" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.resetJSON(msg)
    }, "Reset") : /*#__PURE__*/external_React_default().createElement("button", {
      className: "button show",
      onClick: this.handleOverride(msg.id)
    }, "Show"), isBlocked ? null : /*#__PURE__*/external_React_default().createElement("button", {
      className: "button modify" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.modifyJson(msg)
    }, "Modify"), aboutMessagePreviewSupported ? /*#__PURE__*/external_React_default().createElement(CopyButton, {
      transformer: text => `about:messagepreview?json=${encodeURIComponent(btoa(text))}`,
      label: "Share",
      copiedLabel: "Copied!",
      inputSelector: `#${msg.id}-textarea`,
      className: "button share"
    }) : null, /*#__PURE__*/external_React_default().createElement("br", null), "(", impressions, " impressions)"), /*#__PURE__*/external_React_default().createElement("td", {
      className: "message-summary"
    }, isBlocked && /*#__PURE__*/external_React_default().createElement("tr", null, "Block reason:", isBlockedByGroup && " Blocked by group", isProviderExcluded && " Excluded by provider", isMessageBlocked && " Message blocked"), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("pre", {
      className: isCollapsed ? "collapsed" : "expanded"
    }, /*#__PURE__*/external_React_default().createElement("textarea", {
      id: `${msg.id}-textarea`,
      name: msg.id,
      className: "general-textarea",
      disabled: isBlocked // eslint-disable-next-line react/jsx-no-bind
      ,
      onChange: e => this.handleChange(msg.id)
    }, JSON.stringify(msg, null, 2))))));
  }

  selectPBMessage(msgId) {
    if (msgId === "clear") {
      this.setState({
        selectedPBMessage: ""
      });
    } else {
      let selected = document.getElementById(`${msgId} radio`);
      let msg = JSON.parse(document.getElementById(`${msgId}-textarea`).value);

      if (selected.checked) {
        this.setState({
          selectedPBMessage: msg === null || msg === void 0 ? void 0 : msg.content
        });
      } else {
        this.setState({
          selectedPBMessage: ""
        });
      }
    }
  }

  modifyJson(content) {
    const message = JSON.parse(document.getElementById(`${content.id}-textarea`).value);
    return ASRouterUtils.modifyMessageJson(message).then(state => {
      this.setStateFromParent(state);
      this.props.notifyContent({
        message: state.message
      });
    });
  }

  renderPBMessageItem(msg) {
    const isBlocked = this.state.messageBlockList.includes(msg.id) || this.state.messageBlockList.includes(msg.campaign);
    const impressions = this.state.messageImpressions[msg.id] ? this.state.messageImpressions[msg.id].length : 0;
    const isCollapsed = this.state.collapsedMessages.includes(msg.id);
    let itemClassName = "message-item";

    if (isBlocked) {
      itemClassName += " blocked";
    }

    return /*#__PURE__*/external_React_default().createElement("tr", {
      className: itemClassName,
      key: `${msg.id}-${msg.provider}`
    }, /*#__PURE__*/external_React_default().createElement("td", {
      className: "message-id"
    }, /*#__PURE__*/external_React_default().createElement("span", null, msg.id, " ", /*#__PURE__*/external_React_default().createElement("br", null), /*#__PURE__*/external_React_default().createElement("br", null), "(", impressions, " impressions)")), /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement(ToggleMessageJSON, {
      msgId: `${msg.id}`,
      toggleJSON: this.toggleJSON,
      isCollapsed: isCollapsed
    })), /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("input", {
      type: "radio",
      id: `${msg.id} radio`,
      name: "PB_message_radio",
      style: {
        marginBottom: 20
      },
      onClick: () => this.selectPBMessage(msg.id),
      disabled: isBlocked
    }), /*#__PURE__*/external_React_default().createElement("button", {
      className: `button ${isBlocked ? "" : " primary"}`,
      onClick: isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)
    }, isBlocked ? "Unblock" : "Block"), /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton slim button",
      onClick: e => this.resetPBJSON(msg)
    }, "Reset JSON")), /*#__PURE__*/external_React_default().createElement("td", {
      className: `message-summary`
    }, /*#__PURE__*/external_React_default().createElement("pre", {
      className: isCollapsed ? "collapsed" : "expanded"
    }, /*#__PURE__*/external_React_default().createElement("textarea", {
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

    const messagesToShow = this.state.messageFilter === "all" ? this.state.messages : this.state.messages.filter(message => message.provider === this.state.messageFilter && message.template !== "pb_newtab");
    return /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton slim" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.toggleAllMessages(messagesToShow)
    }, "Collapse/Expand All"), /*#__PURE__*/external_React_default().createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/external_React_default().createElement("span", null, "To modify a message, change the JSON and click 'Modify' to see your changes. Click 'Reset' to restore the JSON to the original. Click 'Share' to copy a link to the clipboard that can be used to preview the message by opening the link in Nightly/local builds.")), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, messagesToShow.map(msg => this.renderMessageItem(msg)))));
  }

  renderMessagesByGroup() {
    if (!this.state.messages) {
      return null;
    }

    const messagesToShow = this.state.messageGroupsFilter === "all" ? this.state.messages.filter(m => m.groups.length) : this.state.messages.filter(message => message.groups.includes(this.state.messageGroupsFilter));
    return /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, messagesToShow.map(msg => this.renderMessageItem(msg))));
  }

  renderPBMessages() {
    if (!this.state.messages) {
      return null;
    }

    const messagesToShow = this.state.messages.filter(message => message.template === "pb_newtab");
    return /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, messagesToShow.map(msg => this.renderPBMessageItem(msg))));
  }

  renderMessageFilter() {
    if (!this.state.providers) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement("p", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "unblock-all ASRouterButton test-only",
      onClick: this.unblockAll
    }, "Unblock All Snippets"), "Show messages from", " ", /*#__PURE__*/external_React_default().createElement("select", {
      value: this.state.messageFilter,
      onChange: this.onChangeMessageFilter
    }, /*#__PURE__*/external_React_default().createElement("option", {
      value: "all"
    }, "all providers"), this.state.providers.map(provider => /*#__PURE__*/external_React_default().createElement("option", {
      key: provider.id,
      value: provider.id
    }, provider.id))), this.state.messageFilter !== "all" && !this.state.messageFilter.includes("_local_testing") ? /*#__PURE__*/external_React_default().createElement("button", {
      className: "button messages-reset",
      onClick: this.handleClearAllImpressionsByProvider
    }, "Reset All") : null);
  }

  renderMessageGroupsFilter() {
    if (!this.state.groups) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement("p", null, "Show messages from ", /*#__PURE__*/external_React_default().createElement("select", {
      value: this.state.messageGroupsFilter,
      onChange: this.onChangeMessageGroupsFilter
    }, /*#__PURE__*/external_React_default().createElement("option", {
      value: "all"
    }, "all groups"), this.state.groups.map(group => /*#__PURE__*/external_React_default().createElement("option", {
      key: group.id,
      value: group.id
    }, group.id))));
  }

  renderTableHead() {
    return /*#__PURE__*/external_React_default().createElement("thead", null, /*#__PURE__*/external_React_default().createElement("tr", {
      className: "message-item"
    }, /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }), /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Provider ID"), /*#__PURE__*/external_React_default().createElement("td", null, "Source"), /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Cohort"), /*#__PURE__*/external_React_default().createElement("td", {
      className: "min"
    }, "Last Updated")));
  }

  renderProviders() {
    const providersConfig = this.state.providerPrefs;
    const providerInfo = this.state.providers;
    const userPrefInfo = this.state.userPrefs;
    return /*#__PURE__*/external_React_default().createElement("table", null, this.renderTableHead(), /*#__PURE__*/external_React_default().createElement("tbody", null, providersConfig.map((provider, i) => {
      const isTestProvider = provider.id.includes("_local_testing");
      const info = providerInfo.find(p => p.id === provider.id) || {};
      const isUserEnabled = provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
      const isSystemEnabled = isTestProvider || provider.enabled;
      let label = "local";

      if (provider.type === "remote") {
        label = /*#__PURE__*/external_React_default().createElement("span", null, "endpoint (", /*#__PURE__*/external_React_default().createElement("a", {
          className: "providerUrl",
          target: "_blank",
          href: info.url,
          rel: "noopener noreferrer"
        }, info.url), ")");
      } else if (provider.type === "remote-settings") {
        label = `remote settings (${provider.collection})`;
      } else if (provider.type === "remote-experiments") {
        label = /*#__PURE__*/external_React_default().createElement("span", null, "remote settings (", /*#__PURE__*/external_React_default().createElement("a", {
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

      return /*#__PURE__*/external_React_default().createElement("tr", {
        className: "message-item",
        key: i
      }, /*#__PURE__*/external_React_default().createElement("td", null, isTestProvider ? /*#__PURE__*/external_React_default().createElement("input", {
        type: "checkbox",
        disabled: true,
        readOnly: true,
        checked: true
      }) : /*#__PURE__*/external_React_default().createElement("input", {
        type: "checkbox",
        "data-provider": provider.id,
        checked: isUserEnabled && isSystemEnabled,
        onChange: this.handleEnabledToggle
      })), /*#__PURE__*/external_React_default().createElement("td", null, provider.id), /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("span", {
        className: `sourceLabel${isUserEnabled && isSystemEnabled ? "" : " isDisabled"}`
      }, label)), /*#__PURE__*/external_React_default().createElement("td", null, provider.cohort), /*#__PURE__*/external_React_default().createElement("td", {
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
    return /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tbody", null, /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("h2", null, "Evaluate JEXL expression"))), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("p", null, /*#__PURE__*/external_React_default().createElement("textarea", {
      ref: "expressionInput",
      rows: "10",
      cols: "60",
      placeholder: "Evaluate JEXL expressions and mock parameters by changing their values below"
    })), /*#__PURE__*/external_React_default().createElement("p", null, "Status:", " ", /*#__PURE__*/external_React_default().createElement("span", {
      ref: "evaluationStatus"
    }, success ? "✅" : "❌", ", Result: ", result))), /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton secondary",
      onClick: this.handleExpressionEval
    }, "Evaluate"))), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("h2", null, "Modify targeting parameters"))), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton secondary",
      onClick: this.onCopyTargetingParams,
      disabled: this.state.copiedToClipboard
    }, this.state.copiedToClipboard ? "Parameters copied!" : "Copy parameters"))), this.state.stringTargetingParameters && Object.keys(this.state.stringTargetingParameters).map((param, i) => {
      const value = this.state.stringTargetingParameters[param];
      const errorState = this.state.targetingParametersError && this.state.targetingParametersError.id === param;
      const className = errorState ? "errorState" : "";
      const inputComp = (value && value.length) > 30 ? /*#__PURE__*/external_React_default().createElement("textarea", {
        name: param,
        className: className,
        value: value,
        rows: "10",
        cols: "60",
        onChange: this.onChangeTargetingParameters
      }) : /*#__PURE__*/external_React_default().createElement("input", {
        name: param,
        className: className,
        value: value,
        onChange: this.onChangeTargetingParameters
      });
      return /*#__PURE__*/external_React_default().createElement("tr", {
        key: i
      }, /*#__PURE__*/external_React_default().createElement("td", null, param), /*#__PURE__*/external_React_default().createElement("td", null, inputComp));
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
    ASRouterUtils.sendMessage({
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

  renderAttributionParamers() {
    return /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("h2", null, " Attribution Parameters "), /*#__PURE__*/external_React_default().createElement("p", null, " ", "This forces the browser to set some attribution parameters, useful for testing the Return To AMO feature. Clicking on 'Force Attribution', with the default values in each field, will demo the Return To AMO flow with the addon called 'uBlock Origin'. If you wish to try different attribution parameters, enter them in the text boxes. If you wish to try a different addon with the Return To AMO flow, make sure the 'content' text box has a string that is 'rta:base64(addonID)', the base64 string of the addonID prefixed with 'rta:'. The addon must currently be a recommended addon on AMO. Then click 'Force Attribution'. Clicking on 'Force Attribution' with blank text boxes reset attribution data."), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Source ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "source",
      placeholder: "addons.mozilla.org",
      value: this.state.attributionParameters.source,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Medium ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "medium",
      placeholder: "referral",
      value: this.state.attributionParameters.medium,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Campaign ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "campaign",
      placeholder: "non-fx-button",
      value: this.state.attributionParameters.campaign,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Content ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "content",
      placeholder: `rta:${btoa("uBlock0@raymondhill.net")}`,
      value: this.state.attributionParameters.content,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Experiment ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "experiment",
      placeholder: "ua-onboarding",
      value: this.state.attributionParameters.experiment,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Variation ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "variation",
      placeholder: "chrome",
      value: this.state.attributionParameters.variation,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " User Agent ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "ua",
      placeholder: "Google Chrome 123",
      value: this.state.attributionParameters.ua,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement("b", null, " Download Token ")), /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      name: "dltoken",
      placeholder: "00000000-0000-0000-0000-000000000000",
      value: this.state.attributionParameters.dltoken,
      onChange: this.onChangeAttributionParameters
    }), " ")), /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("td", null, " ", /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton primary button",
      onClick: this.setAttribution
    }, " ", "Force Attribution", " "), " "))));
  }

  renderErrorMessage({
    id,
    errors
  }) {
    const providerId = /*#__PURE__*/external_React_default().createElement("td", {
      rowSpan: errors.length
    }, id); // .reverse() so that the last error (most recent) is first

    return errors.map(({
      error,
      timestamp
    }, cellKey) => /*#__PURE__*/external_React_default().createElement("tr", {
      key: cellKey
    }, cellKey === errors.length - 1 ? providerId : null, /*#__PURE__*/external_React_default().createElement("td", null, error.message), /*#__PURE__*/external_React_default().createElement("td", null, relativeTime(timestamp)))).reverse();
  }

  renderErrors() {
    const providersWithErrors = this.state.providers && this.state.providers.filter(p => p.errors && p.errors.length);

    if (providersWithErrors && providersWithErrors.length) {
      return /*#__PURE__*/external_React_default().createElement("table", {
        className: "errorReporting"
      }, /*#__PURE__*/external_React_default().createElement("thead", null, /*#__PURE__*/external_React_default().createElement("tr", null, /*#__PURE__*/external_React_default().createElement("th", null, "Provider ID"), /*#__PURE__*/external_React_default().createElement("th", null, "Message"), /*#__PURE__*/external_React_default().createElement("th", null, "Timestamp"))), /*#__PURE__*/external_React_default().createElement("tbody", null, providersWithErrors.map(this.renderErrorMessage)));
    }

    return /*#__PURE__*/external_React_default().createElement("p", null, "No errors");
  }

  renderPBTab() {
    if (!this.state.messages) {
      return null;
    }

    let messagesToShow = this.state.messages.filter(message => message.template === "pb_newtab");
    return /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/external_React_default().createElement("span", null, "To view an available message, select its radio button and click \"Open a Private Browsing Window\".", /*#__PURE__*/external_React_default().createElement("br", null), "To modify a message, make changes to the JSON first, then select the radio button. (To make new changes, click \"Reset Message State\", make your changes, and reselect the radio button.)", /*#__PURE__*/external_React_default().createElement("br", null), "Click \"Reset Message State\" to clear all message impressions and view messages in a clean state.", /*#__PURE__*/external_React_default().createElement("br", null), "Note that ContentSearch functions do not work in debug mode.")), /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton primary button",
      onClick: this.handleOpenPB
    }, "Open a Private Browsing Window"), /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton primary button",
      style: {
        marginInlineStart: 12
      },
      onClick: this.resetPBMessageState
    }, "Reset Message State"), /*#__PURE__*/external_React_default().createElement("br", null), /*#__PURE__*/external_React_default().createElement("input", {
      type: "radio",
      id: `clear radio`,
      name: "PB_message_radio",
      value: "clearPBMessage",
      style: {
        display: "none"
      }
    }), /*#__PURE__*/external_React_default().createElement("h2", null, "Messages"), /*#__PURE__*/external_React_default().createElement("button", {
      className: "ASRouterButton slim button" // eslint-disable-next-line react/jsx-no-bind
      ,
      onClick: e => this.toggleAllMessages(messagesToShow)
    }, "Collapse/Expand All"), this.renderPBMessages()));
  }

  getSection() {
    const [section] = this.props.location.routes;

    switch (section) {
      case "private":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Private Browsing Messages"), this.renderPBTab());

      case "targeting":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Targeting Utilities"), /*#__PURE__*/external_React_default().createElement("button", {
          className: "button",
          onClick: this.expireCache
        }, "Expire Cache"), " ", "(This expires the cache in ASR Targeting for bookmarks and top sites)", this.renderTargetingParameters(), this.renderAttributionParamers());

      case "groups":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Message Groups"), /*#__PURE__*/external_React_default().createElement("button", {
          className: "button",
          onClick: this.resetGroups
        }, "Reset group impressions"), /*#__PURE__*/external_React_default().createElement("table", null, /*#__PURE__*/external_React_default().createElement("thead", null, /*#__PURE__*/external_React_default().createElement("tr", {
          className: "message-item"
        }, /*#__PURE__*/external_React_default().createElement("td", null, "Enabled"), /*#__PURE__*/external_React_default().createElement("td", null, "Impressions count"), /*#__PURE__*/external_React_default().createElement("td", null, "Custom frequency"), /*#__PURE__*/external_React_default().createElement("td", null, "User preferences"))), /*#__PURE__*/external_React_default().createElement("tbody", null, this.state.groups && this.state.groups.map(({
          id,
          enabled,
          frequency,
          userPreferences = []
        }, index) => /*#__PURE__*/external_React_default().createElement(Row, {
          key: id
        }, /*#__PURE__*/external_React_default().createElement("td", null, /*#__PURE__*/external_React_default().createElement(TogglePrefCheckbox, {
          checked: enabled,
          pref: id,
          disabled: true
        })), /*#__PURE__*/external_React_default().createElement("td", null, this._getGroupImpressionsCount(id, frequency)), /*#__PURE__*/external_React_default().createElement("td", null, JSON.stringify(frequency, null, 2)), /*#__PURE__*/external_React_default().createElement("td", null, userPreferences.join(", ")))))), this.renderMessageGroupsFilter(), this.renderMessagesByGroup());

      case "impressions":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Impressions"), /*#__PURE__*/external_React_default().createElement(ImpressionsSection, {
          messageImpressions: this.state.messageImpressions,
          groupImpressions: this.state.groupImpressions,
          screenImpressions: this.state.screenImpressions
        }));

      case "ds":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Discovery Stream"), /*#__PURE__*/external_React_default().createElement(DiscoveryStreamAdmin, {
          state: {
            DiscoveryStream: this.props.DiscoveryStream,
            Personalization: this.props.Personalization
          },
          otherPrefs: this.props.Prefs.values,
          dispatch: this.props.dispatch
        }));

      case "errors":
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "ASRouter Errors"), this.renderErrors());

      default:
        return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", null, "Message Providers", " ", /*#__PURE__*/external_React_default().createElement("button", {
          title: "Restore all provider settings that ship with Firefox",
          className: "button",
          onClick: this.resetPref
        }, "Restore default prefs")), this.state.providers ? this.renderProviders() : null, /*#__PURE__*/external_React_default().createElement("h2", null, "Messages"), this.renderMessageFilter(), this.renderMessages());
    }
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: `asrouter-admin ${this.props.collapsed ? "collapsed" : "expanded"}`
    }, /*#__PURE__*/external_React_default().createElement("aside", {
      className: "sidebar"
    }, /*#__PURE__*/external_React_default().createElement("ul", null, /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools"
    }, "General")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-private"
    }, "Private Browsing")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-targeting"
    }, "Targeting")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-groups"
    }, "Message Groups")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-impressions"
    }, "Impressions")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-ds"
    }, "Discovery Stream")), /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools-errors"
    }, "Errors")))), /*#__PURE__*/external_React_default().createElement("main", {
      className: "main-panel"
    }, /*#__PURE__*/external_React_default().createElement("h1", null, "AS Router Admin"), /*#__PURE__*/external_React_default().createElement("p", {
      className: "helpLink"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), " ", /*#__PURE__*/external_React_default().createElement("span", null, "Need help using these tools? Check out our", " ", /*#__PURE__*/external_React_default().createElement("a", {
      target: "blank",
      href: "https://firefox-source-docs.mozilla.org/browser/components/newtab/content-src/asrouter/docs/debugging-docs.html"
    }, "documentation"))), this.getSection()));
  }

}
class CollapseToggle extends (external_React_default()).PureComponent {
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
      __webpack_require__.g.document.body.classList.add("no-scroll");
    } else {
      __webpack_require__.g.document.body.classList.remove("no-scroll");
    }
  }

  componentDidMount() {
    this.setBodyClass();
  }

  componentDidUpdate() {
    this.setBodyClass();
  }

  componentWillUnmount() {
    __webpack_require__.g.document.body.classList.remove("no-scroll");
    ASRouterUtils.removeListener(this.onMessageFromParent);
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
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("a", {
      href: "#devtools",
      title: label,
      "aria-label": label,
      className: `asrouter-toggle ${isCollapsed ? "collapsed" : "expanded"}`,
      onClick: this.renderAdmin ? this.onCollapseToggle : null
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-devtools"
    })), renderAdmin ? /*#__PURE__*/external_React_default().createElement(ASRouterAdminInner, ASRouterAdmin_extends({}, props, {
      collapsed: this.state.collapsed
    })) : null);
  }

}

const _ASRouterAdmin = props => /*#__PURE__*/external_React_default().createElement(SimpleHashRouter, null, /*#__PURE__*/external_React_default().createElement(CollapseToggle, props));

const ASRouterAdmin = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Personalization: state.Personalization,
  Prefs: state.Prefs
}))(_ASRouterAdmin);
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/types.js
/**
 * The `FluentType` class is the base of Fluent's type system.
 *
 * Fluent types wrap JavaScript values and store additional configuration for
 * them, which can then be used in the `toString` method together with a proper
 * `Intl` formatter.
 */
class FluentType {
  /**
   * Create a `FluentType` instance.
   *
   * @param value The JavaScript value to wrap.
   */
  constructor(value) {
    this.value = value;
  }
  /**
   * Unwrap the raw value stored by this `FluentType`.
   */


  valueOf() {
    return this.value;
  }

}
/**
 * A `FluentType` representing no correct value.
 */

class FluentNone extends FluentType {
  /**
   * Create an instance of `FluentNone` with an optional fallback value.
   * @param value The fallback value of this `FluentNone`.
   */
  constructor(value = "???") {
    super(value);
  }
  /**
   * Format this `FluentNone` to the fallback string.
   */


  toString(scope) {
    return `{${this.value}}`;
  }

}
/**
 * A `FluentType` representing a number.
 *
 * A `FluentNumber` instance stores the number value of the number it
 * represents. It may also store an option bag of options which will be passed
 * to `Intl.NumerFormat` when the `FluentNumber` is formatted to a string.
 */

class FluentNumber extends FluentType {
  /**
   * Create an instance of `FluentNumber` with options to the
   * `Intl.NumberFormat` constructor.
   *
   * @param value The number value of this `FluentNumber`.
   * @param opts Options which will be passed to `Intl.NumberFormat`.
   */
  constructor(value, opts = {}) {
    super(value);
    this.opts = opts;
  }
  /**
   * Format this `FluentNumber` to a string.
   */


  toString(scope) {
    try {
      const nf = scope.memoizeIntlObject(Intl.NumberFormat, this.opts);
      return nf.format(this.value);
    } catch (err) {
      scope.reportError(err);
      return this.value.toString(10);
    }
  }

}
/**
 * A `FluentType` representing a date and time.
 *
 * A `FluentDateTime` instance stores the number value of the date it
 * represents, as a numerical timestamp in milliseconds. It may also store an
 * option bag of options which will be passed to `Intl.DateTimeFormat` when the
 * `FluentDateTime` is formatted to a string.
 */

class FluentDateTime extends FluentType {
  /**
   * Create an instance of `FluentDateTime` with options to the
   * `Intl.DateTimeFormat` constructor.
   *
   * @param value The number value of this `FluentDateTime`, in milliseconds.
   * @param opts Options which will be passed to `Intl.DateTimeFormat`.
   */
  constructor(value, opts = {}) {
    super(value);
    this.opts = opts;
  }
  /**
   * Format this `FluentDateTime` to a string.
   */


  toString(scope) {
    try {
      const dtf = scope.memoizeIntlObject(Intl.DateTimeFormat, this.opts);
      return dtf.format(this.value);
    } catch (err) {
      scope.reportError(err);
      return new Date(this.value).toISOString();
    }
  }

}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/resolver.js
/* global Intl */

/**
 * @overview
 *
 * The role of the Fluent resolver is to format a `Pattern` to an instance of
 * `FluentValue`. For performance reasons, primitive strings are considered
 * such instances, too.
 *
 * Translations can contain references to other messages or variables,
 * conditional logic in form of select expressions, traits which describe their
 * grammatical features, and can use Fluent builtins which make use of the
 * `Intl` formatters to format numbers and dates into the bundle's languages.
 * See the documentation of the Fluent syntax for more information.
 *
 * In case of errors the resolver will try to salvage as much of the
 * translation as possible. In rare situations where the resolver didn't know
 * how to recover from an error it will return an instance of `FluentNone`.
 *
 * All expressions resolve to an instance of `FluentValue`. The caller should
 * use the `toString` method to convert the instance to a native value.
 *
 * Functions in this file pass around an instance of the `Scope` class, which
 * stores the data required for successful resolution and error recovery.
 */
 // The maximum number of placeables which can be expanded in a single call to
// `formatPattern`. The limit protects against the Billion Laughs and Quadratic
// Blowup attacks. See https://msdn.microsoft.com/en-us/magazine/ee335713.aspx.

const MAX_PLACEABLES = 100; // Unicode bidi isolation characters.

const FSI = "\u2068";
const PDI = "\u2069"; // Helper: match a variant key to the given selector.

function match(scope, selector, key) {
  if (key === selector) {
    // Both are strings.
    return true;
  } // XXX Consider comparing options too, e.g. minimumFractionDigits.


  if (key instanceof FluentNumber && selector instanceof FluentNumber && key.value === selector.value) {
    return true;
  }

  if (selector instanceof FluentNumber && typeof key === "string") {
    let category = scope.memoizeIntlObject(Intl.PluralRules, selector.opts).select(selector.value);

    if (key === category) {
      return true;
    }
  }

  return false;
} // Helper: resolve the default variant from a list of variants.


function getDefault(scope, variants, star) {
  if (variants[star]) {
    return resolvePattern(scope, variants[star].value);
  }

  scope.reportError(new RangeError("No default"));
  return new FluentNone();
} // Helper: resolve arguments to a call expression.


function getArguments(scope, args) {
  const positional = [];
  const named = Object.create(null);

  for (const arg of args) {
    if (arg.type === "narg") {
      named[arg.name] = resolveExpression(scope, arg.value);
    } else {
      positional.push(resolveExpression(scope, arg));
    }
  }

  return {
    positional,
    named
  };
} // Resolve an expression to a Fluent type.


function resolveExpression(scope, expr) {
  switch (expr.type) {
    case "str":
      return expr.value;

    case "num":
      return new FluentNumber(expr.value, {
        minimumFractionDigits: expr.precision
      });

    case "var":
      return resolveVariableReference(scope, expr);

    case "mesg":
      return resolveMessageReference(scope, expr);

    case "term":
      return resolveTermReference(scope, expr);

    case "func":
      return resolveFunctionReference(scope, expr);

    case "select":
      return resolveSelectExpression(scope, expr);

    default:
      return new FluentNone();
  }
} // Resolve a reference to a variable.


function resolveVariableReference(scope, {
  name
}) {
  let arg;

  if (scope.params) {
    // We're inside a TermReference. It's OK to reference undefined parameters.
    if (Object.prototype.hasOwnProperty.call(scope.params, name)) {
      arg = scope.params[name];
    } else {
      return new FluentNone(`$${name}`);
    }
  } else if (scope.args && Object.prototype.hasOwnProperty.call(scope.args, name)) {
    // We're in the top-level Pattern or inside a MessageReference. Missing
    // variables references produce ReferenceErrors.
    arg = scope.args[name];
  } else {
    scope.reportError(new ReferenceError(`Unknown variable: $${name}`));
    return new FluentNone(`$${name}`);
  } // Return early if the argument already is an instance of FluentType.


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
        return new FluentDateTime(arg.getTime());
      }

    // eslint-disable-next-line no-fallthrough

    default:
      scope.reportError(new TypeError(`Variable type not supported: $${name}, ${typeof arg}`));
      return new FluentNone(`$${name}`);
  }
} // Resolve a reference to another message.


function resolveMessageReference(scope, {
  name,
  attr
}) {
  const message = scope.bundle._messages.get(name);

  if (!message) {
    scope.reportError(new ReferenceError(`Unknown message: ${name}`));
    return new FluentNone(name);
  }

  if (attr) {
    const attribute = message.attributes[attr];

    if (attribute) {
      return resolvePattern(scope, attribute);
    }

    scope.reportError(new ReferenceError(`Unknown attribute: ${attr}`));
    return new FluentNone(`${name}.${attr}`);
  }

  if (message.value) {
    return resolvePattern(scope, message.value);
  }

  scope.reportError(new ReferenceError(`No value: ${name}`));
  return new FluentNone(name);
} // Resolve a call to a Term with key-value arguments.


function resolveTermReference(scope, {
  name,
  attr,
  args
}) {
  const id = `-${name}`;

  const term = scope.bundle._terms.get(id);

  if (!term) {
    scope.reportError(new ReferenceError(`Unknown term: ${id}`));
    return new FluentNone(id);
  }

  if (attr) {
    const attribute = term.attributes[attr];

    if (attribute) {
      // Every TermReference has its own variables.
      scope.params = getArguments(scope, args).named;
      const resolved = resolvePattern(scope, attribute);
      scope.params = null;
      return resolved;
    }

    scope.reportError(new ReferenceError(`Unknown attribute: ${attr}`));
    return new FluentNone(`${id}.${attr}`);
  }

  scope.params = getArguments(scope, args).named;
  const resolved = resolvePattern(scope, term.value);
  scope.params = null;
  return resolved;
} // Resolve a call to a Function with positional and key-value arguments.


function resolveFunctionReference(scope, {
  name,
  args
}) {
  // Some functions are built-in. Others may be provided by the runtime via
  // the `FluentBundle` constructor.
  let func = scope.bundle._functions[name];

  if (!func) {
    scope.reportError(new ReferenceError(`Unknown function: ${name}()`));
    return new FluentNone(`${name}()`);
  }

  if (typeof func !== "function") {
    scope.reportError(new TypeError(`Function ${name}() is not callable`));
    return new FluentNone(`${name}()`);
  }

  try {
    let resolved = getArguments(scope, args);
    return func(resolved.positional, resolved.named);
  } catch (err) {
    scope.reportError(err);
    return new FluentNone(`${name}()`);
  }
} // Resolve a select expression to the member object.


function resolveSelectExpression(scope, {
  selector,
  variants,
  star
}) {
  let sel = resolveExpression(scope, selector);

  if (sel instanceof FluentNone) {
    return getDefault(scope, variants, star);
  } // Match the selector against keys of each variant, in order.


  for (const variant of variants) {
    const key = resolveExpression(scope, variant.key);

    if (match(scope, sel, key)) {
      return resolvePattern(scope, variant.value);
    }
  }

  return getDefault(scope, variants, star);
} // Resolve a pattern (a complex string with placeables).


function resolveComplexPattern(scope, ptn) {
  if (scope.dirty.has(ptn)) {
    scope.reportError(new RangeError("Cyclic reference"));
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

    scope.placeables++;

    if (scope.placeables > MAX_PLACEABLES) {
      scope.dirty.delete(ptn); // This is a fatal error which causes the resolver to instantly bail out
      // on this pattern. The length check protects against excessive memory
      // usage, and throwing protects against eating up the CPU when long
      // placeables are deeply nested.

      throw new RangeError(`Too many placeables expanded: ${scope.placeables}, ` + `max allowed is ${MAX_PLACEABLES}`);
    }

    if (useIsolating) {
      result.push(FSI);
    }

    result.push(resolveExpression(scope, elem).toString(scope));

    if (useIsolating) {
      result.push(PDI);
    }
  }

  scope.dirty.delete(ptn);
  return result.join("");
} // Resolve a simple or a complex Pattern to a FluentString (which is really the
// string primitive).

function resolvePattern(scope, value) {
  // Resolve a simple pattern.
  if (typeof value === "string") {
    return scope.bundle._transform(value);
  }

  return resolveComplexPattern(scope, value);
}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/scope.js
class Scope {
  constructor(bundle, errors, args) {
    /** The Set of patterns already encountered during this resolution.
     * Used to detect and prevent cyclic resolutions. */
    this.dirty = new WeakSet();
    /** A dict of parameters passed to a TermReference. */

    this.params = null;
    /** The running count of placeables resolved so far. Used to detect the
      * Billion Laughs and Quadratic Blowup attacks. */

    this.placeables = 0;
    this.bundle = bundle;
    this.errors = errors;
    this.args = args;
  }

  reportError(error) {
    if (!this.errors || !(error instanceof Error)) {
      throw error;
    }

    this.errors.push(error);
  }

  memoizeIntlObject(ctor, opts) {
    let cache = this.bundle._intls.get(ctor);

    if (!cache) {
      cache = {};

      this.bundle._intls.set(ctor, cache);
    }

    let id = JSON.stringify(opts);

    if (!cache[id]) {
      cache[id] = new ctor(this.bundle.locales, opts);
    }

    return cache[id];
  }

}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/builtins.js
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
 * `FluentValue`.  Functions must return `FluentValues` as well.
 */


function values(opts, allowed) {
  const unwrapped = Object.create(null);

  for (const [name, opt] of Object.entries(opts)) {
    if (allowed.includes(name)) {
      unwrapped[name] = opt.valueOf();
    }
  }

  return unwrapped;
}

const NUMBER_ALLOWED = ["unitDisplay", "currencyDisplay", "useGrouping", "minimumIntegerDigits", "minimumFractionDigits", "maximumFractionDigits", "minimumSignificantDigits", "maximumSignificantDigits"];
/**
 * The implementation of the `NUMBER()` builtin available to translations.
 *
 * Translations may call the `NUMBER()` builtin in order to specify formatting
 * options of a number. For example:
 *
 *     pi = The value of π is {NUMBER($pi, maximumFractionDigits: 2)}.
 *
 * The implementation expects an array of `FluentValues` representing the
 * positional arguments, and an object of named `FluentValues` representing the
 * named parameters.
 *
 * The following options are recognized:
 *
 *     unitDisplay
 *     currencyDisplay
 *     useGrouping
 *     minimumIntegerDigits
 *     minimumFractionDigits
 *     maximumFractionDigits
 *     minimumSignificantDigits
 *     maximumSignificantDigits
 *
 * Other options are ignored.
 *
 * @param args The positional arguments passed to this `NUMBER()`.
 * @param opts The named argments passed to this `NUMBER()`.
 */

function NUMBER(args, opts) {
  let arg = args[0];

  if (arg instanceof FluentNone) {
    return new FluentNone(`NUMBER(${arg.valueOf()})`);
  }

  if (arg instanceof FluentNumber) {
    return new FluentNumber(arg.valueOf(), { ...arg.opts,
      ...values(opts, NUMBER_ALLOWED)
    });
  }

  if (arg instanceof FluentDateTime) {
    return new FluentNumber(arg.valueOf(), { ...values(opts, NUMBER_ALLOWED)
    });
  }

  throw new TypeError("Invalid argument to NUMBER");
}
const DATETIME_ALLOWED = ["dateStyle", "timeStyle", "fractionalSecondDigits", "dayPeriod", "hour12", "weekday", "era", "year", "month", "day", "hour", "minute", "second", "timeZoneName"];
/**
 * The implementation of the `DATETIME()` builtin available to translations.
 *
 * Translations may call the `DATETIME()` builtin in order to specify
 * formatting options of a number. For example:
 *
 *     now = It's {DATETIME($today, month: "long")}.
 *
 * The implementation expects an array of `FluentValues` representing the
 * positional arguments, and an object of named `FluentValues` representing the
 * named parameters.
 *
 * The following options are recognized:
 *
 *     dateStyle
 *     timeStyle
 *     fractionalSecondDigits
 *     dayPeriod
 *     hour12
 *     weekday
 *     era
 *     year
 *     month
 *     day
 *     hour
 *     minute
 *     second
 *     timeZoneName
 *
 * Other options are ignored.
 *
 * @param args The positional arguments passed to this `DATETIME()`.
 * @param opts The named argments passed to this `DATETIME()`.
 */

function DATETIME(args, opts) {
  let arg = args[0];

  if (arg instanceof FluentNone) {
    return new FluentNone(`DATETIME(${arg.valueOf()})`);
  }

  if (arg instanceof FluentDateTime) {
    return new FluentDateTime(arg.valueOf(), { ...arg.opts,
      ...values(opts, DATETIME_ALLOWED)
    });
  }

  if (arg instanceof FluentNumber) {
    return new FluentDateTime(arg.valueOf(), { ...values(opts, DATETIME_ALLOWED)
    });
  }

  throw new TypeError("Invalid argument to DATETIME");
}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/memoizer.js
const cache = new Map();
function getMemoizerForLocale(locales) {
  const stringLocale = Array.isArray(locales) ? locales.join(" ") : locales;
  let memoizer = cache.get(stringLocale);

  if (memoizer === undefined) {
    memoizer = new Map();
    cache.set(stringLocale, memoizer);
  }

  return memoizer;
}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/bundle.js





/**
 * Message bundles are single-language stores of translation resources. They are
 * responsible for formatting message values and attributes to strings.
 */

class FluentBundle {
  /**
   * Create an instance of `FluentBundle`.
   *
   * The `locales` argument is used to instantiate `Intl` formatters used by
   * translations. The `options` object can be used to configure the bundle.
   *
   * Examples:
   *
   *     let bundle = new FluentBundle(["en-US", "en"]);
   *
   *     let bundle = new FluentBundle(locales, {useIsolating: false});
   *
   *     let bundle = new FluentBundle(locales, {
   *       useIsolating: true,
   *       functions: {
   *         NODE_ENV: () => process.env.NODE_ENV
   *       }
   *     });
   *
   * Available options:
   *
   *   - `functions` - an object of additional functions available to
   *     translations as builtins.
   *
   *   - `useIsolating` - boolean specifying whether to use Unicode isolation
   *     marks (FSI, PDI) for bidi interpolations. Default: `true`.
   *
   *   - `transform` - a function used to transform string parts of patterns.
   */
  constructor(locales, {
    functions,
    useIsolating = true,
    transform = v => v
  } = {}) {
    this._terms = new Map();
    this._messages = new Map();
    this.locales = Array.isArray(locales) ? locales : [locales];
    this._functions = {
      NUMBER: NUMBER,
      DATETIME: DATETIME,
      ...functions
    };
    this._useIsolating = useIsolating;
    this._transform = transform;
    this._intls = getMemoizerForLocale(locales);
  }
  /**
   * Check if a message is present in the bundle.
   *
   * @param id - The identifier of the message to check.
   */


  hasMessage(id) {
    return this._messages.has(id);
  }
  /**
   * Return a raw unformatted message object from the bundle.
   *
   * Raw messages are `{value, attributes}` shapes containing translation units
   * called `Patterns`. `Patterns` are implementation-specific; they should be
   * treated as black boxes and formatted with `FluentBundle.formatPattern`.
   *
   * @param id - The identifier of the message to check.
   */


  getMessage(id) {
    return this._messages.get(id);
  }
  /**
   * Add a translation resource to the bundle.
   *
   * The translation resource must be an instance of `FluentResource`.
   *
   *     let res = new FluentResource("foo = Foo");
   *     bundle.addResource(res);
   *     bundle.getMessage("foo");
   *     // → {value: .., attributes: {..}}
   *
   * Available options:
   *
   *   - `allowOverrides` - boolean specifying whether it's allowed to override
   *     an existing message or term with a new value. Default: `false`.
   *
   * @param   res - FluentResource object.
   * @param   options
   */


  addResource(res, {
    allowOverrides = false
  } = {}) {
    const errors = [];

    for (let i = 0; i < res.body.length; i++) {
      let entry = res.body[i];

      if (entry.id.startsWith("-")) {
        // Identifiers starting with a dash (-) define terms. Terms are private
        // and cannot be retrieved from FluentBundle.
        if (allowOverrides === false && this._terms.has(entry.id)) {
          errors.push(new Error(`Attempt to override an existing term: "${entry.id}"`));
          continue;
        }

        this._terms.set(entry.id, entry);
      } else {
        if (allowOverrides === false && this._messages.has(entry.id)) {
          errors.push(new Error(`Attempt to override an existing message: "${entry.id}"`));
          continue;
        }

        this._messages.set(entry.id, entry);
      }
    }

    return errors;
  }
  /**
   * Format a `Pattern` to a string.
   *
   * Format a raw `Pattern` into a string. `args` will be used to resolve
   * references to variables passed as arguments to the translation.
   *
   * In case of errors `formatPattern` will try to salvage as much of the
   * translation as possible and will still return a string. For performance
   * reasons, the encountered errors are not returned but instead are appended
   * to the `errors` array passed as the third argument.
   *
   *     let errors = [];
   *     bundle.addResource(
   *         new FluentResource("hello = Hello, {$name}!"));
   *
   *     let hello = bundle.getMessage("hello");
   *     if (hello.value) {
   *         bundle.formatPattern(hello.value, {name: "Jane"}, errors);
   *         // Returns "Hello, Jane!" and `errors` is empty.
   *
   *         bundle.formatPattern(hello.value, undefined, errors);
   *         // Returns "Hello, {$name}!" and `errors` is now:
   *         // [<ReferenceError: Unknown variable: name>]
   *     }
   *
   * If `errors` is omitted, the first encountered error will be thrown.
   */


  formatPattern(pattern, args = null, errors = null) {
    // Resolve a simple pattern without creating a scope. No error handling is
    // required; by definition simple patterns don't have placeables.
    if (typeof pattern === "string") {
      return this._transform(pattern);
    } // Resolve a complex pattern.


    let scope = new Scope(this, errors, args);

    try {
      let value = resolveComplexPattern(scope, pattern);
      return value.toString(scope);
    } catch (err) {
      if (scope.errors && err instanceof Error) {
        scope.errors.push(err);
        return new FluentNone().toString(scope);
      }

      throw err;
    }
  }

}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/resource.js
// This regex is used to iterate through the beginnings of messages and terms.
// With the /m flag, the ^ matches at the beginning of every line.
const RE_MESSAGE_START = /^(-?[a-zA-Z][\w-]*) *= */gm; // Both Attributes and Variants are parsed in while loops. These regexes are
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
const TOKEN_BLANK = /\s+/y;
/**
 * Fluent Resource is a structure storing parsed localization entries.
 */

class FluentResource {
  constructor(source) {
    this.body = [];
    RE_MESSAGE_START.lastIndex = 0;
    let cursor = 0; // Iterate over the beginnings of messages and terms to efficiently skip
    // comments and recover from errors.

    while (true) {
      let next = RE_MESSAGE_START.exec(source);

      if (next === null) {
        break;
      }

      cursor = RE_MESSAGE_START.lastIndex;

      try {
        this.body.push(parseMessage(next[1]));
      } catch (err) {
        if (err instanceof SyntaxError) {
          // Don't report any Fluent syntax errors. Skip directly to the
          // beginning of the next message or term.
          continue;
        }

        throw err;
      }
    } // The parser implementation is inlined below for performance reasons,
    // as well as for convenience of accessing `source` and `cursor`.
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
        throw new SyntaxError(`Expected ${re.toString()}`);
      }

      cursor = re.lastIndex;
      return result;
    } // Execute a regex, advance the cursor, and return the capture group.


    function match1(re) {
      return match(re)[1];
    }

    function parseMessage(id) {
      let value = parsePattern();
      let attributes = parseAttributes();

      if (value === null && Object.keys(attributes).length === 0) {
        throw new SyntaxError("Expected message value or attributes");
      }

      return {
        id,
        value,
        attributes
      };
    }

    function parseAttributes() {
      let attrs = Object.create(null);

      while (test(RE_ATTRIBUTE_START)) {
        let name = match1(RE_ATTRIBUTE_START);
        let value = parsePattern();

        if (value === null) {
          throw new SyntaxError("Expected attribute value");
        }

        attrs[name] = value;
      }

      return attrs;
    }

    function parsePattern() {
      let first; // First try to parse any simple text on the same line as the id.

      if (test(RE_TEXT_RUN)) {
        first = match1(RE_TEXT_RUN);
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
      while (true) {
        if (test(RE_TEXT_RUN)) {
          elements.push(match1(RE_TEXT_RUN));
          continue;
        }

        if (source[cursor] === "{") {
          elements.push(parsePlaceable());
          continue;
        }

        if (source[cursor] === "}") {
          throw new SyntaxError("Unbalanced closing brace");
        }

        let indent = parseIndent();

        if (indent) {
          elements.push(indent);
          commonIndent = Math.min(commonIndent, indent.length);
          continue;
        }

        break;
      }

      let lastIndex = elements.length - 1;
      let lastElement = elements[lastIndex]; // Trim the trailing spaces in the last element if it's a TextElement.

      if (typeof lastElement === "string") {
        elements[lastIndex] = trim(lastElement, RE_TRAILING_SPACES);
      }

      let baked = [];

      for (let element of elements) {
        if (element instanceof Indent) {
          // Dedent indented lines by the maximum common indent.
          element = element.value.slice(0, element.value.length - commonIndent);
        }

        if (element) {
          baked.push(element);
        }
      }

      return baked;
    }

    function parsePlaceable() {
      consumeToken(TOKEN_BRACE_OPEN, SyntaxError);
      let selector = parseInlineExpression();

      if (consumeToken(TOKEN_BRACE_CLOSE)) {
        return selector;
      }

      if (consumeToken(TOKEN_ARROW)) {
        let variants = parseVariants();
        consumeToken(TOKEN_BRACE_CLOSE, SyntaxError);
        return {
          type: "select",
          selector,
          ...variants
        };
      }

      throw new SyntaxError("Unclosed placeable");
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

          throw new SyntaxError("Function names must be all upper-case");
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
            throw new SyntaxError("Unclosed argument list");
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
          throw new SyntaxError("Expected variant value");
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
        throw new SyntaxError("Expected default variant");
      }

      return {
        variants,
        star
      };
    }

    function parseVariantKey() {
      consumeToken(TOKEN_BRACKET_OPEN, SyntaxError);
      let key;

      if (test(RE_NUMBER_LITERAL)) {
        key = parseNumberLiteral();
      } else {
        key = {
          type: "str",
          value: match1(RE_IDENTIFIER)
        };
      }

      consumeToken(TOKEN_BRACKET_CLOSE, SyntaxError);
      return key;
    }

    function parseLiteral() {
      if (test(RE_NUMBER_LITERAL)) {
        return parseNumberLiteral();
      }

      if (source[cursor] === '"') {
        return parseStringLiteral();
      }

      throw new SyntaxError("Invalid expression");
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
      consumeChar('"', SyntaxError);
      let value = "";

      while (true) {
        value += match1(RE_STRING_RUN);

        if (source[cursor] === "\\") {
          value += parseEscapeSequence();
          continue;
        }

        if (consumeChar('"')) {
          return {
            type: "str",
            value
          };
        } // We've reached an EOL of EOF.


        throw new SyntaxError("Unclosed string literal");
      }
    } // Unescape known escape sequences.


    function parseEscapeSequence() {
      if (test(RE_STRING_ESCAPE)) {
        return match1(RE_STRING_ESCAPE);
      }

      if (test(RE_UNICODE_ESCAPE)) {
        let [, codepoint4, codepoint6] = match(RE_UNICODE_ESCAPE);
        let codepoint = parseInt(codepoint4 || codepoint6, 16);
        return codepoint <= 0xd7ff || 0xe000 <= codepoint // It's a Unicode scalar value.
        ? String.fromCodePoint(codepoint) // Lonely surrogates can cause trouble when the parsing result is
        // saved using UTF-8. Use U+FFFD REPLACEMENT CHARACTER instead.
        : "�";
      }

      throw new SyntaxError("Unknown escape sequence");
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
      let value = blank.replace(RE_BLANK_LINES, "\n"); // eslint-disable-next-line @typescript-eslint/no-non-null-assertion

      let length = RE_INDENT.exec(blank)[1].length;
      return new Indent(value, length);
    }
  }

}

class Indent {
  constructor(value, length) {
    this.value = value;
    this.length = length;
  }

}
;// CONCATENATED MODULE: ./node_modules/@fluent/bundle/esm/index.js
/**
 * @module fluent
 * @overview
 *
 * `fluent` is a JavaScript implementation of Project Fluent, a localization
 * framework designed to unleash the expressive power of the natural language.
 *
 */



;// CONCATENATED MODULE: ./content-src/asrouter/rich-text-strings.js
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
  const bundle = new FluentBundle("en-US");
  RICH_TEXT_KEYS.forEach(key => {
    const attrs = RICH_TEXT_CONFIG[key];
    const attrsToTry = Array.isArray(attrs) ? [...attrs] : [attrs];
    let string = "";

    while (!string && attrsToTry.length) {
      const attr = attrsToTry.pop();
      string = content[attr];
    }

    bundle.addResource(new FluentResource(`${key} = ${string}`));
  });
  return [bundle];
}
;// CONCATENATED MODULE: ./content-src/asrouter/components/ImpressionsWrapper/ImpressionsWrapper.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
/**
 * Component wrapper used to send telemetry pings on every impression.
 */

class ImpressionsWrapper extends (external_React_default()).PureComponent {
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
  document: __webpack_require__.g.document,
  sendOnMount: true
};
;// CONCATENATED MODULE: ./node_modules/@fluent/sequence/esm/map_sync.js
/**
 * Synchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param bundles - An iterable of bundles to sift through.
 * @param ids - An id or ids to map.
 */
function mapBundleSync(bundles, ids) {
  if (!Array.isArray(ids)) {
    return getBundleForId(bundles, ids);
  }

  return ids.map(id => getBundleForId(bundles, id));
}
/*
 * Find the best `FluentBundle` with the translation for `id`.
 */

function getBundleForId(bundles, id) {
  for (const bundle of bundles) {
    if (bundle.hasMessage(id)) {
      return bundle;
    }
  }

  return null;
}
;// CONCATENATED MODULE: ./node_modules/@fluent/sequence/esm/map_async.js
/**
 * Asynchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param bundles - An iterable of bundles to sift through.
 * @param ids - An id or ids to map.
 */
async function mapBundleAsync(bundles, ids) {
  if (!Array.isArray(ids)) {
    for await (const bundle of bundles) {
      if (bundle.hasMessage(ids)) {
        return bundle;
      }
    }

    return null;
  }

  const foundBundles = new Array(ids.length).fill(null);
  let remainingCount = ids.length;

  for await (const bundle of bundles) {
    for (const [index, id] of ids.entries()) {
      if (!foundBundles[index] && bundle.hasMessage(id)) {
        foundBundles[index] = bundle;
        remainingCount--;
      } // Return early when all ids have been mapped to bundles.


      if (remainingCount === 0) {
        return foundBundles;
      }
    }
  }

  return foundBundles;
}
;// CONCATENATED MODULE: ./node_modules/@fluent/sequence/esm/index.js
/**
 * @module fluent-sequence
 * @overview Manage ordered sequences of FluentBundles.
 */


;// CONCATENATED MODULE: ./node_modules/@fluent/react/node_modules/cached-iterable/src/cached_iterable.mjs
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

;// CONCATENATED MODULE: ./node_modules/@fluent/react/node_modules/cached-iterable/src/cached_sync_iterable.mjs


/*
 * CachedSyncIterable caches the elements yielded by an iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedSyncIterable extends CachedIterable {
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

;// CONCATENATED MODULE: ./node_modules/@fluent/react/node_modules/cached-iterable/src/cached_async_iterable.mjs


/*
 * CachedAsyncIterable caches the elements yielded by an async iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedAsyncIterable extends CachedIterable {
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
    async touchNext(count = 1) {
        let idx = 0;
        while (idx++ < count) {
            const last = this[this.length - 1];
            if (last && (await last).done) {
                break;
            }
            this.push(this.iterator.next());
        }
        // Return the last cached {value, done} object to allow the calling
        // code to decide if it needs to call touchNext again.
        return this[this.length - 1];
    }
}

;// CONCATENATED MODULE: ./node_modules/@fluent/react/node_modules/cached-iterable/src/index.mjs



;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/markup.js
let cachedParseMarkup;
/**
 * We use a function creator to make the reference to `document` lazy. At the
 * same time, it's eager enough to throw in `<LocalizationProvider>` as soon as
 * it's first mounted which reduces the risk of this error making it to the
 * runtime without developers noticing it in development.
 */

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
;// CONCATENATED MODULE: ./node_modules/@fluent/react/vendor/omittedCloseTags.js
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
/* harmony default export */ const vendor_omittedCloseTags = (omittedCloseTags);
;// CONCATENATED MODULE: ./node_modules/@fluent/react/vendor/voidElementTags.js
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
/* harmony default export */ const vendor_voidElementTags = (voidElementTags);
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/localization.js




 // Match the opening angle bracket (<) in HTML tags, and HTML entities like
// &amp;, &#0038;, &#x0026;.

const reMarkup = /<|&#?\w+;/;
/**
 * `ReactLocalization` handles translation formatting and fallback.
 *
 * The current negotiated fallback chain of languages is stored in the
 * `ReactLocalization` instance in form of an iterable of `FluentBundle`
 * instances. This iterable is used to find the best existing translation for
 * a given identifier.
 *
 * The `ReactLocalization` class instances are exposed to `Localized` elements
 * via the `LocalizationProvider` component.
 */

class ReactLocalization {
  constructor(bundles, parseMarkup = createParseMarkup()) {
    this.bundles = CachedSyncIterable.from(bundles);
    this.parseMarkup = parseMarkup;
  }

  getBundle(id) {
    return mapBundleSync(this.bundles, id);
  }

  areBundlesEmpty() {
    // Create an iterator and only peek at the first value to see if it contains
    // anything.
    return Boolean(this.bundles[Symbol.iterator]().next().done);
  }

  getString(id, vars, fallback) {
    const bundle = this.getBundle(id);

    if (bundle) {
      const msg = bundle.getMessage(id);

      if (msg && msg.value) {
        let errors = [];
        let value = bundle.formatPattern(msg.value, vars, errors);

        for (let error of errors) {
          this.reportError(error);
        }

        return value;
      }
    } else {
      if (this.areBundlesEmpty()) {
        this.reportError(new Error("Attempting to get a string when no localization bundles are " + "present."));
      } else {
        this.reportError(new Error(`The id "${id}" did not match any messages in the localization ` + "bundles."));
      }
    }

    return fallback || id;
  }

  getElement(sourceElement, id, args = {}) {
    const bundle = this.getBundle(id);

    if (bundle === null) {
      if (!id) {
        this.reportError(new Error("No string id was provided when localizing a component."));
      } else if (this.areBundlesEmpty()) {
        this.reportError(new Error("Attempting to get a localized element when no localization bundles are " + "present."));
      } else {
        this.reportError(new Error(`The id "${id}" did not match any messages in the localization ` + "bundles."));
      }

      return /*#__PURE__*/(0,external_React_namespaceObject.createElement)(external_React_namespaceObject.Fragment, null, sourceElement);
    } // this.getBundle makes the bundle.hasMessage check which ensures that
    // bundle.getMessage returns an existing message.
    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion


    const msg = bundle.getMessage(id);
    let errors = [];
    let localizedProps; // The default is to forbid all message attributes. If the attrs prop exists
    // on the Localized instance, only set message attributes which have been
    // explicitly allowed by the developer.

    if (args.attrs && msg.attributes) {
      localizedProps = {};
      errors = [];

      for (const [name, allowed] of Object.entries(args.attrs)) {
        if (allowed && name in msg.attributes) {
          localizedProps[name] = bundle.formatPattern(msg.attributes[name], args.vars, errors);
        }
      }

      for (let error of errors) {
        this.reportError(error);
      }
    } // If the component to render is a known void element, explicitly dismiss the
    // message value and do not pass it to cloneElement in order to avoid the
    // "void element tags must neither have `children` nor use
    // `dangerouslySetInnerHTML`" error.


    if (typeof sourceElement.type === "string" && sourceElement.type in vendor_voidElementTags) {
      return /*#__PURE__*/(0,external_React_namespaceObject.cloneElement)(sourceElement, localizedProps);
    } // If the message has a null value, we're only interested in its attributes.
    // Do not pass the null value to cloneElement as it would nuke all children
    // of the wrapped component.


    if (msg.value === null) {
      return /*#__PURE__*/(0,external_React_namespaceObject.cloneElement)(sourceElement, localizedProps);
    }

    errors = [];
    const messageValue = bundle.formatPattern(msg.value, args.vars, errors);

    for (let error of errors) {
      this.reportError(error);
    } // If the message value doesn't contain any markup nor any HTML entities,
    // insert it as the only child of the component to render.


    if (!reMarkup.test(messageValue) || this.parseMarkup === null) {
      return /*#__PURE__*/(0,external_React_namespaceObject.cloneElement)(sourceElement, localizedProps, messageValue);
    }

    let elemsLower;

    if (args.elems) {
      elemsLower = new Map();

      for (let [name, elem] of Object.entries(args.elems)) {
        // Ignore elems which are not valid React elements.
        if (! /*#__PURE__*/(0,external_React_namespaceObject.isValidElement)(elem)) {
          continue;
        }

        elemsLower.set(name.toLowerCase(), elem);
      }
    } // If the message contains markup, parse it and try to match the children
    // found in the translation with the args passed to this function.


    const translationNodes = this.parseMarkup(messageValue);
    const translatedChildren = translationNodes.map(({
      nodeName,
      textContent
    }) => {
      if (nodeName === "#text") {
        return textContent;
      }

      const childName = nodeName.toLowerCase();
      const sourceChild = elemsLower === null || elemsLower === void 0 ? void 0 : elemsLower.get(childName); // If the child is not expected just take its textContent.

      if (!sourceChild) {
        return textContent;
      } // If the element passed in the elems prop is a known void element,
      // explicitly dismiss any textContent which might have accidentally been
      // defined in the translation to prevent the "void element tags must not
      // have children" error.


      if (typeof sourceChild.type === "string" && sourceChild.type in vendor_voidElementTags) {
        return sourceChild;
      } // TODO Protect contents of elements wrapped in <Localized>
      // https://github.com/projectfluent/fluent.js/issues/184
      // TODO  Control localizable attributes on elements passed as props
      // https://github.com/projectfluent/fluent.js/issues/185


      return /*#__PURE__*/(0,external_React_namespaceObject.cloneElement)(sourceChild, undefined, textContent);
    });
    return /*#__PURE__*/(0,external_React_namespaceObject.cloneElement)(sourceElement, localizedProps, ...translatedChildren);
  } // XXX Control this via a prop passed to the LocalizationProvider.
  // See https://github.com/projectfluent/fluent.js/issues/411.


  reportError(error) {
    /* global console */
    // eslint-disable-next-line no-console
    console.warn(`[@fluent/react] ${error.name}: ${error.message}`);
  }

}
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/context.js

let FluentContext = /*#__PURE__*/(0,external_React_namespaceObject.createContext)(null);
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/provider.js


/**
 * The Provider component for the `ReactLocalization` class.
 *
 * Exposes a `ReactLocalization` instance to all descendants via React's
 * context feature.  It makes translations available to all localizable
 * elements in the descendant's render tree without the need to pass them
 * explicitly.
 *
 * `LocalizationProvider` takes an instance of `ReactLocalization` in the
 * `l10n` prop. This instance will be made available to `Localized` components
 * under the provider.
 *
 * @example
 * ```jsx
 * <LocalizationProvider l10n={…}>
 *     …
 * </LocalizationProvider>
 * ```
 */

function LocalizationProvider(props) {
  return /*#__PURE__*/(0,external_React_namespaceObject.createElement)(FluentContext.Provider, {
    value: props.l10n
  }, props.children);
}
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/with_localization.js


function withLocalization(Inner) {
  function WithLocalization(props) {
    const l10n = (0,external_React_namespaceObject.useContext)(FluentContext);

    if (!l10n) {
      throw new Error("withLocalization was used without wrapping it in a " + "<LocalizationProvider />.");
    } // Re-bind getString to trigger a re-render of Inner.


    const getString = l10n.getString.bind(l10n);
    return /*#__PURE__*/(0,external_React_namespaceObject.createElement)(Inner, {
      getString,
      ...props
    });
  }

  WithLocalization.displayName = `WithLocalization(${displayName(Inner)})`;
  return WithLocalization;
}

function displayName(component) {
  return component.displayName || component.name || "Component";
}
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/localized.js


/**
 * The `Localized` class renders its child with translated props and children.
 *
 * The `id` prop should be the unique identifier of the translation.  Any
 * attributes found in the translation will be applied to the wrapped element.
 *
 * Arguments to the translation can be passed as `$`-prefixed props on
 * `Localized`.
 *
 * It's recommended that the contents of the wrapped component be a string
 * expression.  The string will be used as the ultimate fallback if no
 * translation is available.  It also makes it easy to grep for strings in the
 * source code.
 *
 * @example
 * ```jsx
 * <Localized id="hello-world">
 *     <p>{'Hello, world!'}</p>
 * </Localized>
 *
 * <Localized id="hello-world" $username={name}>
 *     <p>{'Hello, { $username }!'}</p>
 * </Localized>
 * ```
 */

function Localized(props) {
  const {
    id,
    attrs,
    vars,
    elems,
    children
  } = props;
  const l10n = (0,external_React_namespaceObject.useContext)(FluentContext);

  if (!l10n) {
    throw new Error("The <Localized /> component was not properly wrapped in a <LocalizationProvider />.");
  }

  let source;

  if (Array.isArray(children)) {
    if (children.length > 1) {
      throw new Error("Expected to receive a single React element to localize.");
    } // If it's an array with zero or one element, we can directly get the first one.
    // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment


    source = children[0];
  } else {
    source = children !== null && children !== void 0 ? children : null;
  } // Check if the component to render is a valid element -- if not, then
  // it's either null or a simple fallback string. No need to localize the
  // attributes or replace.


  if (! /*#__PURE__*/(0,external_React_namespaceObject.isValidElement)(source)) {
    const fallback = typeof source === "string" ? source : undefined;
    const string = l10n.getString(id, vars, fallback);
    return /*#__PURE__*/external_React_namespaceObject.createElement(external_React_namespaceObject.Fragment, null, string);
  }

  return l10n.getElement(source, id, {
    attrs,
    vars,
    elems
  });
}
/* harmony default export */ const localized = (Localized);
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/use_localization.js


const useLocalization = () => {
  const l10n = (0,external_React_namespaceObject.useContext)(FluentContext);

  if (!l10n) {
    throw new Error("useLocalization was used without wrapping it in a " + "<LocalizationProvider />.");
  }

  return {
    l10n
  };
};
;// CONCATENATED MODULE: ./node_modules/@fluent/react/esm/index.js
/**
 * @module fluent-react
 * @overview
 *

 * `fluent-react` provides React bindings for Fluent.  It takes advantage of
 * React's Components system and the virtual DOM.  Translations are exposed to
 * components via the provider pattern.
 *
 * Consult the documentation of the `LocalizationProvider` and the `Localized`
 * components for more information.
 *
 * @example
 * ```jsx
 * <LocalizationProvider l10n={…}>
 *     <Localized id="hello-world">
 *         <p>{'Hello, world!'}</p>
 *     </Localized>
 * </LocalizationProvider>
 * ```
 */





;// CONCATENATED MODULE: ./content-src/lib/constants.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const IS_NEWTAB = __webpack_require__.g.document && __webpack_require__.g.document.documentURI === "about:newtab";
const NEWTAB_DARK_THEME = {
  ntp_background: {
    r: 42,
    g: 42,
    b: 46,
    a: 1
  },
  ntp_card_background: {
    r: 66,
    g: 65,
    b: 77,
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
;// CONCATENATED MODULE: external "ReactDOM"
const external_ReactDOM_namespaceObject = ReactDOM;
var external_ReactDOM_default = /*#__PURE__*/__webpack_require__.n(external_ReactDOM_namespaceObject);
;// CONCATENATED MODULE: ./content-src/asrouter/components/Button/Button.jsx
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

  return /*#__PURE__*/external_React_default().createElement("button", {
    onClick: props.onClick,
    className: props.className || "ASRouterButton secondary",
    style: style
  }, props.children);
};
;// CONCATENATED MODULE: ./content-src/asrouter/components/ConditionalWrapper/ConditionalWrapper.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// lifted from https://gist.github.com/kitze/23d82bb9eb0baabfd03a6a720b1d637f
const ConditionalWrapper = ({
  condition,
  wrap,
  children
}) => condition && wrap ? wrap(children) : children;

/* harmony default export */ const ConditionalWrapper_ConditionalWrapper = (ConditionalWrapper);
;// CONCATENATED MODULE: ./content-src/asrouter/template-utils.js
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
    // eslint-disable-next-line no-console
    console.warn(`The protocol ${protocol} is not allowed for template URLs.`);
  }

  return isAllowed ? url : "";
}
;// CONCATENATED MODULE: ./content-src/asrouter/components/RichText/RichText.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



 // Elements allowed in snippet content

const ALLOWED_TAGS = {
  b: /*#__PURE__*/external_React_default().createElement("b", null),
  i: /*#__PURE__*/external_React_default().createElement("i", null),
  u: /*#__PURE__*/external_React_default().createElement("u", null),
  strong: /*#__PURE__*/external_React_default().createElement("strong", null),
  em: /*#__PURE__*/external_React_default().createElement("em", null),
  br: /*#__PURE__*/external_React_default().createElement("br", null)
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
      external_React_default().createElement("a", {
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
  if (!RICH_TEXT_KEYS.includes(props.localization_id)) {
    throw new Error(`ASRouter: ${props.localization_id} is not a valid rich text property. If you want it to be processed, you need to add it to asrouter/rich-text-strings.js`);
  }

  return /*#__PURE__*/external_React_default().createElement(Localized, {
    id: props.localization_id,
    elems: { ...ALLOWED_TAGS,
      ...props.customElements,
      ...convertLinks(props.links, props.sendClick, props.doNotAutoBlock, props.openNewWindow)
    }
  }, /*#__PURE__*/external_React_default().createElement("span", null, props.text));
}
;// CONCATENATED MODULE: ./content-src/asrouter/components/SnippetBase/SnippetBase.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SnippetBase extends (external_React_default()).PureComponent {
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
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "footer"
      }, /*#__PURE__*/external_React_default().createElement("div", {
        className: "footer-content"
      }, /*#__PURE__*/external_React_default().createElement("button", {
        className: "ASRouterButton secondary",
        onClick: this.onDismissClicked
      }, this.props.content.scene2_dismiss_button_text)));
    }

    const label = this.props.content.block_button_text || "Remove this";
    return /*#__PURE__*/external_React_default().createElement("button", {
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
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: containerClassName,
      style: this.props.textStyle
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "innerWrapper"
    }, props.children), this.renderDismissButton());
  }

}
;// CONCATENATED MODULE: ./content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx
function SimpleSnippet_extends() { SimpleSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SimpleSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png"; // Alt text placeholder in case the prop from the server isn't available

const ICON_ALT_TEXT = "";
class SimpleSnippet extends (external_React_default()).PureComponent {
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
    return title ? /*#__PURE__*/external_React_default().createElement("h3", {
      className: `title ${this._shouldRenderButton() ? "title-inline" : ""}`
    }, this.renderTitleIcon(), " ", title) : null;
  }

  renderTitleIcon() {
    const titleIconLight = safeURI(this.props.content.title_icon);
    const titleIconDark = safeURI(this.props.content.title_icon_dark_theme || this.props.content.title_icon);

    if (!titleIconLight) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("span", {
      className: "titleIcon icon-light-theme",
      style: {
        backgroundImage: `url("${titleIconLight}")`
      }
    }), /*#__PURE__*/external_React_default().createElement("span", {
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

    return /*#__PURE__*/external_React_default().createElement(Button, {
      onClick: props.onButtonClick || this.onButtonClick,
      color: props.content.button_color,
      backgroundColor: props.content.button_background_color
    }, props.content.button_label);
  }

  renderText() {
    const {
      props
    } = this;
    return /*#__PURE__*/external_React_default().createElement(RichText, {
      text: props.content.text,
      customElements: this.props.customElements,
      localization_id: "text",
      links: props.content.links,
      sendClick: props.sendClick
    });
  }

  wrapSectionHeader(url) {
    return function (children) {
      return /*#__PURE__*/external_React_default().createElement("a", {
        href: url
      }, children);
    };
  }

  wrapSnippetContent(children) {
    return /*#__PURE__*/external_React_default().createElement("div", {
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
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "section-header"
      }, /*#__PURE__*/external_React_default().createElement("h3", {
        className: "section-title"
      }, /*#__PURE__*/external_React_default().createElement(ConditionalWrapper_ConditionalWrapper, {
        condition: sectionTitleURL,
        wrap: this.wrapSectionHeader(sectionTitleURL)
      }, /*#__PURE__*/external_React_default().createElement("span", {
        className: "icon icon-small-spacer icon-light-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconLight}")`
        }
      }), /*#__PURE__*/external_React_default().createElement("span", {
        className: "icon icon-small-spacer icon-dark-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconDark}")`
        }
      }), /*#__PURE__*/external_React_default().createElement("span", {
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

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "snippet-hover-wrapper"
    }, /*#__PURE__*/external_React_default().createElement(SnippetBase, SimpleSnippet_extends({}, props, {
      className: className,
      textStyle: this.props.textStyle
    }), sectionHeader, /*#__PURE__*/external_React_default().createElement(ConditionalWrapper_ConditionalWrapper, {
      condition: sectionHeader,
      wrap: this.wrapSnippetContent
    }, /*#__PURE__*/external_React_default().createElement("img", {
      src: safeURI(props.content.icon) || DEFAULT_ICON_PATH,
      className: "icon icon-light-theme",
      alt: props.content.icon_alt_text || ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default().createElement("img", {
      src: safeURI(props.content.icon_dark_theme || props.content.icon) || DEFAULT_ICON_PATH,
      className: "icon icon-dark-theme",
      alt: props.content.icon_alt_text || ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default().createElement("div", null, this.renderTitle(), " ", /*#__PURE__*/external_React_default().createElement("p", {
      className: "body"
    }, this.renderText()), this.props.extraContent), /*#__PURE__*/external_React_default().createElement("div", null, this.renderButton()))));
  }

}
;// CONCATENATED MODULE: ./content-src/asrouter/templates/EOYSnippet/EOYSnippet.jsx
function EOYSnippet_extends() { EOYSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return EOYSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class EOYSnippetBase extends (external_React_default()).PureComponent {
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

    return /*#__PURE__*/external_React_default().createElement("form", {
      className: "EOYSnippetForm",
      action: this.props.content.donation_form_url,
      method: this.props.form_method,
      onSubmit: this.handleSubmit,
      "data-metric": "EOYSnippetForm",
      ref: "form"
    }, donationURLParams.map(([key, value], idx) => /*#__PURE__*/external_React_default().createElement("input", {
      type: "hidden",
      name: key,
      value: value,
      key: idx
    })), fieldNames.map((field, idx) => {
      const button_name = `donation_amount_${field}`;
      const amount = this.props.content[button_name];
      return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, {
        key: idx
      }, /*#__PURE__*/external_React_default().createElement("input", {
        type: "radio",
        name: "amount",
        value: amount,
        id: field,
        defaultChecked: button_name === selected_button
      }), /*#__PURE__*/external_React_default().createElement("label", {
        htmlFor: field,
        className: "donation-amount"
      }, numberFormat.format(amount)));
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "monthly-checkbox-container"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      id: "monthly-checkbox",
      type: "checkbox"
    }), /*#__PURE__*/external_React_default().createElement("label", {
      htmlFor: "monthly-checkbox"
    }, this.props.content.monthly_checkbox_label_text)), /*#__PURE__*/external_React_default().createElement("input", {
      type: "hidden",
      name: "frequency",
      value: "single"
    }), /*#__PURE__*/external_React_default().createElement("input", {
      type: "hidden",
      name: "currency",
      value: this.props.content.currency_code
    }), /*#__PURE__*/external_React_default().createElement("input", {
      type: "hidden",
      name: "presets",
      value: fieldNames.map(field => this.props.content[`donation_amount_${field}`])
    }), /*#__PURE__*/external_React_default().createElement("button", {
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
    const customElement = /*#__PURE__*/external_React_default().createElement("em", {
      style: {
        backgroundColor: this.props.content.highlight_color
      }
    });
    return /*#__PURE__*/external_React_default().createElement(SimpleSnippet, EOYSnippet_extends({}, this.props, {
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
  return /*#__PURE__*/external_React_default().createElement(EOYSnippetBase, EOYSnippet_extends({}, props, {
    content: extendedContent,
    form_method: "GET"
  }));
};
;// CONCATENATED MODULE: ./content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.jsx
function SubmitFormSnippet_extends() { SubmitFormSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SubmitFormSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






 // Alt text placeholder in case the prop from the server isn't available

const SubmitFormSnippet_ICON_ALT_TEXT = "";
class SubmitFormSnippet extends (external_React_default()).PureComponent {
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
      console.error(err);
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

    return Object.keys(hidden_inputs).map((key, idx) => /*#__PURE__*/external_React_default().createElement("input", {
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

    return /*#__PURE__*/external_React_default().createElement("p", {
      className: "disclaimerText"
    }, /*#__PURE__*/external_React_default().createElement(RichText, {
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

    return /*#__PURE__*/external_React_default().createElement("p", {
      className: "privacyNotice"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      type: "checkbox",
      id: "id_privacy",
      name: "privacy",
      required: "required"
    }), /*#__PURE__*/external_React_default().createElement("label", {
      htmlFor: "id_privacy"
    }, /*#__PURE__*/external_React_default().createElement(RichText, {
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
    return /*#__PURE__*/external_React_default().createElement(SnippetBase, this.props, /*#__PURE__*/external_React_default().createElement("div", {
      className: "submissionStatus"
    }, successTitle ? /*#__PURE__*/external_React_default().createElement("h2", {
      className: "submitStatusTitle"
    }, successTitle) : null, /*#__PURE__*/external_React_default().createElement("p", null, /*#__PURE__*/external_React_default().createElement(RichText, SubmitFormSnippet_extends({}, bodyText, {
      localization_id: isSuccess ? "success_text" : "error_text"
    })), isSuccess ? null : /*#__PURE__*/external_React_default().createElement(Button, {
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
      return /*#__PURE__*/external_React_default().createElement("a", {
        href: url
      }, children);
    };
  }

  renderInput() {
    const placholder = this.props.content.scene2_email_placeholder_text || this.props.content.scene2_input_placeholder;
    return /*#__PURE__*/external_React_default().createElement("input", {
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
    return /*#__PURE__*/external_React_default().createElement("form", {
      action: this.props.form_action,
      method: this.props.form_method,
      onSubmit: this.handleSubmit,
      ref: "form"
    }, this.renderHiddenFormInputs(), /*#__PURE__*/external_React_default().createElement("div", null, this.renderInput(), /*#__PURE__*/external_React_default().createElement("button", {
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

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "scene2Icon"
    }, /*#__PURE__*/external_React_default().createElement("img", {
      src: safeURI(content.scene2_icon),
      className: "icon-light-theme",
      alt: content.scene2_icon_alt_text || SubmitFormSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default().createElement("img", {
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
    return /*#__PURE__*/external_React_default().createElement(SnippetBase, SubmitFormSnippet_extends({}, this.props, {
      className: containerClass,
      footerDismiss: true
    }), this.renderScene2Icon(), /*#__PURE__*/external_React_default().createElement("div", {
      className: "message"
    }, /*#__PURE__*/external_React_default().createElement("p", null, content.scene2_title && /*#__PURE__*/external_React_default().createElement("h3", {
      className: "scene2Title"
    }, content.scene2_title), " ", content.scene2_text && /*#__PURE__*/external_React_default().createElement(RichText, {
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
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "section-header"
      }, /*#__PURE__*/external_React_default().createElement("h3", {
        className: "section-title"
      }, /*#__PURE__*/external_React_default().createElement(ConditionalWrapper_ConditionalWrapper, {
        wrap: this.wrapSectionHeader(sectionTitleURL),
        condition: sectionTitleURL
      }, /*#__PURE__*/external_React_default().createElement("span", {
        className: "icon icon-small-spacer icon-light-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconLight}")`
        }
      }), /*#__PURE__*/external_React_default().createElement("span", {
        className: "icon icon-small-spacer icon-dark-theme",
        style: {
          backgroundImage: `url("${sectionTitleIconDark}")`
        }
      }), /*#__PURE__*/external_React_default().createElement("span", {
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
    return /*#__PURE__*/external_React_default().createElement(SnippetBase, SubmitFormSnippet_extends({}, this.props, {
      className: containerClass // Don't show bottom dismiss button
      ,
      footerDismiss: false
    }), this.renderSectionHeader(), this.renderScene2Icon(), /*#__PURE__*/external_React_default().createElement("div", {
      className: "message"
    }, /*#__PURE__*/external_React_default().createElement("p", null, content.scene2_text && /*#__PURE__*/external_React_default().createElement(RichText, {
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

    return /*#__PURE__*/external_React_default().createElement(SimpleSnippet, SubmitFormSnippet_extends({}, this.props, {
      content: content,
      onButtonClick: this.expandSnippet
    }));
  }

}
;// CONCATENATED MODULE: ./content-src/asrouter/templates/FXASignupSnippet/FXASignupSnippet.jsx
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
  return /*#__PURE__*/external_React_default().createElement(SubmitFormSnippet, FXASignupSnippet_extends({}, props, {
    content: extendedContent,
    form_action: "https://accounts.firefox.com/",
    form_method: "GET"
  }));
};
;// CONCATENATED MODULE: ./content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet.jsx
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
  return /*#__PURE__*/external_React_default().createElement(SubmitFormSnippet, NewsletterSnippet_extends({}, props, {
    content: extendedContent,
    form_action: "https://basket.mozilla.org/subscribe.json",
    form_method: "POST"
  }));
};
;// CONCATENATED MODULE: ./content-src/asrouter/templates/SendToDeviceSnippet/isEmailOrPhoneNumber.js
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
;// CONCATENATED MODULE: ./content-src/asrouter/templates/SendToDeviceSnippet/SendToDeviceSnippet.jsx
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
  return /*#__PURE__*/external_React_default().createElement(SubmitFormSnippet, SendToDeviceSnippet_extends({}, propsWithDefaults, {
    form_method: "POST",
    className: "send_to_device_snippet",
    inputType: propsWithDefaults.content.include_sms ? "text" : "email",
    validateInput: propsWithDefaults.content.include_sms ? validateInput : null,
    processFormData: processFormData
  }));
};
const SendToDeviceScene2Snippet = props => {
  return /*#__PURE__*/external_React_default().createElement(SendToDeviceSnippet, SendToDeviceSnippet_extends({
    expandedAlt: true
  }, props));
};
;// CONCATENATED MODULE: ./content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.jsx
function SimpleBelowSearchSnippet_extends() { SimpleBelowSearchSnippet_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return SimpleBelowSearchSnippet_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





const SimpleBelowSearchSnippet_DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png"; // Alt text placeholder in case the prop from the server isn't available

const SimpleBelowSearchSnippet_ICON_ALT_TEXT = "";
class SimpleBelowSearchSnippet extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  renderText() {
    const {
      props
    } = this;
    return props.content.text ? /*#__PURE__*/external_React_default().createElement(RichText, {
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
    return title ? /*#__PURE__*/external_React_default().createElement("h3", {
      className: "title title-inline"
    }, title, /*#__PURE__*/external_React_default().createElement("br", null)) : null;
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

    return /*#__PURE__*/external_React_default().createElement(Button, {
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

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: containerName
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "snippet-hover-wrapper"
    }, /*#__PURE__*/external_React_default().createElement(SnippetBase, SimpleBelowSearchSnippet_extends({}, props, {
      className: className,
      textStyle: this.props.textStyle
    }), /*#__PURE__*/external_React_default().createElement("img", {
      src: safeURI(props.content.icon) || SimpleBelowSearchSnippet_DEFAULT_ICON_PATH,
      className: "icon icon-light-theme",
      alt: props.content.icon_alt_text || SimpleBelowSearchSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default().createElement("img", {
      src: safeURI(props.content.icon_dark_theme || props.content.icon) || SimpleBelowSearchSnippet_DEFAULT_ICON_PATH,
      className: "icon icon-dark-theme",
      alt: props.content.icon_alt_text || SimpleBelowSearchSnippet_ICON_ALT_TEXT
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "textContainer"
    }, this.renderTitle(), /*#__PURE__*/external_React_default().createElement("p", {
      className: "body"
    }, this.renderText()), this.props.extraContent), /*#__PURE__*/external_React_default().createElement("div", {
      className: "buttonContainer"
    }, this.renderButton()))));
  }

}
;// CONCATENATED MODULE: ./content-src/asrouter/templates/template-manifest.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





 // Key names matching schema name of templates

const SnippetsTemplates = {
  simple_snippet: SimpleSnippet,
  newsletter_snippet: NewsletterSnippet,
  fxa_signup_snippet: FXASignupSnippet,
  send_to_device_snippet: SendToDeviceSnippet,
  send_to_device_scene2_snippet: SendToDeviceScene2Snippet,
  eoy_snippet: EOYSnippet,
  simple_below_search_snippet: SimpleBelowSearchSnippet
};
;// CONCATENATED MODULE: ./content-src/asrouter/asrouter-content.jsx
function asrouter_content_extends() { asrouter_content_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return asrouter_content_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */










const TEMPLATES_BELOW_SEARCH = ["simple_below_search_snippet"]; // Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />

function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id);
}

class ASRouterUISurface extends (external_React_default()).PureComponent {
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
      console.error(err);
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
        console.error("Non-200 response", response);
      }
    } catch (error) {
      console.error(error);
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
    ASRouterUtils.sendTelemetry({
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
    return ASRouterUtils.sendMessage({
      type: MESSAGE_TYPE_HASH.IMPRESSION,
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
      ASRouterUtils.executeAction(action);
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
    return ASRouterUtils.blockById(id, options).then(clearAll => {
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
            type: actionTypes.SNIPPETS_PREVIEW_MODE
          });
          break;
        }
    }
  }

  requestMessage(endpoint) {
    ASRouterUtils.sendMessage({
      type: "NEWTAB_MESSAGE_REQUEST",
      data: {
        endpoint
      }
    }).then(state => this.setState(state));
  }

  componentWillMount() {
    const endpoint = ASRouterUtils.getPreviewEndpoint();

    if (endpoint && endpoint.theme === "dark") {
      __webpack_require__.g.window.dispatchEvent(new CustomEvent("LightweightTheme:Set", {
        detail: {
          data: NEWTAB_DARK_THEME
        }
      }));
    }

    if (endpoint && endpoint.dir === "rtl") {
      //Set `dir = rtl` on the HTML
      this.props.document.dir = "rtl";
    }

    ASRouterUtils.addListener(this.onMessageFromParent);
    this.requestMessage(endpoint);
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  componentDidUpdate(prevProps, prevState) {
    if (prevProps.adminContent && JSON.stringify(prevProps.adminContent) !== JSON.stringify(this.props.adminContent)) {
      this.updateContent();
    }

    if (prevState.message.id !== this.state.message.id) {
      const main = __webpack_require__.g.window.document.querySelector("main");

      if (main) {
        if (this.state.message.id) {
          main.classList.add("has-snippet");
        } else {
          main.classList.remove("has-snippet");
        }
      }
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
        ASRouterUtils.executeAction({
          type: "OPEN_URL",
          data: {
            args: url
          }
        });
        break;

      default:
        ASRouterUtils.executeAction(action);
    }
  }

  renderSnippets() {
    const {
      message
    } = this.state;

    if (!SnippetsTemplates[message.template]) {
      return null;
    }

    const SnippetComponent = SnippetsTemplates[message.template];
    const {
      content
    } = message;
    return /*#__PURE__*/external_React_default().createElement(ImpressionsWrapper, {
      id: "NEWTAB_FOOTER_BAR",
      message: this.state.message,
      sendImpression: this.sendImpression,
      shouldSendImpressionOnUpdate: shouldSendImpressionOnUpdate // This helps with testing
      ,
      document: this.props.document
    }, /*#__PURE__*/external_React_default().createElement(LocalizationProvider, {
      l10n: new ReactLocalization(generateBundles(content))
    }, /*#__PURE__*/external_React_default().createElement(SnippetComponent, asrouter_content_extends({}, this.state.message, {
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

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "snippets-preview-banner"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-small-spacer icon-info"
    }), /*#__PURE__*/external_React_default().createElement("span", null, "Preview Purposes Only"));
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
    external_React_default().createElement("div", {
      className: "below-search-snippet-wrapper"
    }, this.renderSnippets()) :
    /*#__PURE__*/
    // For regular snippets etc. we should render everything in our footer
    // container.
    external_ReactDOM_default().createPortal( /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, this.renderPreviewBanner(), this.renderSnippets()), this.footerPortal);
  }

}
ASRouterUISurface.defaultProps = {
  document: __webpack_require__.g.document
};
;// CONCATENATED MODULE: ./content-src/components/ConfirmDialog/ConfirmDialog.jsx
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

class _ConfirmDialog extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({
      type: actionTypes.DIALOG_CANCEL
    });
    this.props.dispatch(actionCreators.UserEvent({
      event: actionTypes.DIALOG_CANCEL,
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

    return /*#__PURE__*/external_React_default().createElement("span", null, message_body.map(msg => /*#__PURE__*/external_React_default().createElement("p", {
      key: msg,
      "data-l10n-id": msg
    })));
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "confirmation-dialog"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "modal-overlay",
      onClick: this._handleCancelBtn,
      role: "presentation"
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "modal"
    }, /*#__PURE__*/external_React_default().createElement("section", {
      className: "modal-message"
    }, this.props.data.icon && /*#__PURE__*/external_React_default().createElement("span", {
      className: `icon icon-spacer icon-${this.props.data.icon}`
    }), this._renderModalMessage()), /*#__PURE__*/external_React_default().createElement("section", {
      className: "actions"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      onClick: this._handleCancelBtn,
      "data-l10n-id": this.props.data.cancel_button_string_id
    }), /*#__PURE__*/external_React_default().createElement("button", {
      className: "done",
      onClick: this._handleConfirmBtn,
      "data-l10n-id": this.props.data.confirm_button_string_id
    }))));
  }

}
const ConfirmDialog = (0,external_ReactRedux_namespaceObject.connect)(state => state.Dialog)(_ConfirmDialog);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSImage/DSImage.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PLACEHOLDER_IMAGE_DATA_ARRAY = [{
  rotation: "0deg",
  offsetx: "20px",
  offsety: "8px",
  scale: "45%"
}, {
  rotation: "54deg",
  offsetx: "-26px",
  offsety: "62px",
  scale: "55%"
}, {
  rotation: "-30deg",
  offsetx: "78px",
  offsety: "30px",
  scale: "68%"
}, {
  rotation: "-22deg",
  offsetx: "0",
  offsety: "92px",
  scale: "60%"
}, {
  rotation: "-65deg",
  offsetx: "66px",
  offsety: "28px",
  scale: "60%"
}, {
  rotation: "22deg",
  offsetx: "-35px",
  offsety: "62px",
  scale: "52%"
}, {
  rotation: "-25deg",
  offsetx: "86px",
  offsety: "-15px",
  scale: "68%"
}];
const PLACEHOLDER_IMAGE_COLORS_ARRAY = "#0090ED #FF4F5F #2AC3A2 #FF7139 #A172FF #FFA437 #FF2A8A".split(" ");

function generateIndex({
  keyCode,
  max
}) {
  if (!keyCode) {
    // Just grab a random index if we cannot generate an index from a key.
    return Math.floor(Math.random() * max);
  }

  const hashStr = str => {
    let hash = 0;

    for (let i = 0; i < str.length; i++) {
      let charCode = str.charCodeAt(i);
      hash += charCode;
    }

    return hash;
  };

  const hash = hashStr(keyCode);
  return hash % max;
}

function PlaceholderImage({
  urlKey,
  titleKey
}) {
  const dataIndex = generateIndex({
    keyCode: urlKey,
    max: PLACEHOLDER_IMAGE_DATA_ARRAY.length
  });
  const colorIndex = generateIndex({
    keyCode: titleKey,
    max: PLACEHOLDER_IMAGE_COLORS_ARRAY.length
  });
  const {
    rotation,
    offsetx,
    offsety,
    scale
  } = PLACEHOLDER_IMAGE_DATA_ARRAY[dataIndex];
  const color = PLACEHOLDER_IMAGE_COLORS_ARRAY[colorIndex];
  const style = {
    "--placeholderBackgroundColor": color,
    "--placeholderBackgroundRotation": rotation,
    "--placeholderBackgroundOffsetx": offsetx,
    "--placeholderBackgroundOffsety": offsety,
    "--placeholderBackgroundScale": scale
  };
  return /*#__PURE__*/external_React_default().createElement("div", {
    style: style,
    className: "placeholder-image"
  });
}
class DSImage extends (external_React_default()).PureComponent {
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

        img = /*#__PURE__*/external_React_default().createElement("img", {
          loading: "lazy",
          alt: this.props.alt_text,
          crossOrigin: "anonymous",
          onLoad: this.onLoad,
          onError: this.onOptimizedImageError,
          sizes: sizeRules.join(","),
          src: baseSource,
          srcSet: srcSetRules.join(",")
        });
      } else if (this.props.source && !this.state.nonOptimizedImageFailed) {
        img = /*#__PURE__*/external_React_default().createElement("img", {
          loading: "lazy",
          alt: this.props.alt_text,
          crossOrigin: "anonymous",
          onLoad: this.onLoad,
          onError: this.onNonOptimizedImageError,
          src: this.props.source
        });
      } else {
        // We consider a failed to load img or source without an image as loaded.
        classNames = `${classNames} loaded`; // Remove the img element if we have no source. Render a placeholder instead.
        // This only happens for recent saves without a source.

        if (this.props.isRecentSave && !this.props.rawSource && !this.props.source) {
          img = /*#__PURE__*/external_React_default().createElement(PlaceholderImage, {
            urlKey: this.props.url,
            titleKey: this.props.title
          });
        } else {
          img = /*#__PURE__*/external_React_default().createElement("div", {
            className: "broken-image"
          });
        }
      }
    }

    return /*#__PURE__*/external_React_default().createElement("picture", {
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
;// CONCATENATED MODULE: ./content-src/components/ContextMenu/ContextMenu.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class ContextMenu extends (external_React_default()).PureComponent {
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
      __webpack_require__.g.addEventListener("click", this.hideContext);
    }, 0);
  }

  componentWillUnmount() {
    __webpack_require__.g.removeEventListener("click", this.hideContext);
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
      external_React_default().createElement("span", {
        className: "context-menu"
      }, /*#__PURE__*/external_React_default().createElement("ul", {
        role: "menu",
        onClick: this.onClick,
        onKeyDown: this.onClick,
        className: "context-menu-list"
      }, this.props.options.map((option, i) => option.type === "separator" ? /*#__PURE__*/external_React_default().createElement("li", {
        key: i,
        className: "separator",
        role: "separator"
      }) : option.type !== "empty" && /*#__PURE__*/external_React_default().createElement(ContextMenuItem, {
        key: i,
        option: option,
        hideContext: this.hideContext,
        keyboardAccess: this.props.keyboardAccess
      }))))
    );
  }

}
class _ContextMenuItem extends (external_React_default()).PureComponent {
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
    return /*#__PURE__*/external_React_default().createElement("li", {
      role: "presentation",
      className: "context-menu-item"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: option.disabled ? "disabled" : "",
      role: "menuitem",
      onClick: this.onClick,
      onKeyDown: this.onKeyDown,
      onKeyUp: this.onKeyUp,
      ref: option.first ? this.focusFirst : null,
      "aria-haspopup": option.id === "newtab-menu-edit-topsites" ? "dialog" : null
    }, /*#__PURE__*/external_React_default().createElement("span", {
      "data-l10n-id": option.string_id || option.id
    })));
  }

}
const ContextMenuItem = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Prefs: state.Prefs
}))(_ContextMenuItem);
;// CONCATENATED MODULE: ./content-src/lib/link-menu-options.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const _OpenInPrivateWindow = site => ({
  id: "newtab-menu-open-new-private-window",
  icon: "new-window-private",
  action: actionCreators.OnlyToMain({
    type: actionTypes.OPEN_PRIVATE_WINDOW,
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
      type: actionTypes.SHOW_PRIVACY_INFO
    },
    userEvent: "SHOW_PRIVACY_INFO"
  }),
  AboutSponsored: site => ({
    id: "newtab-menu-show-privacy-info",
    icon: "info",
    action: actionCreators.AlsoToMain({
      type: actionTypes.ABOUT_SPONSORED_TOP_SITES,
      data: {
        advertiser_name: (site.label || site.hostname).toLocaleLowerCase(),
        position: site.sponsored_position,
        tile_id: site.sponsored_tile_id
      }
    }),
    userEvent: "TOPSITE_SPONSOR_INFO"
  }),
  RemoveBookmark: site => ({
    id: "newtab-menu-remove-bookmark",
    icon: "bookmark-added",
    action: actionCreators.AlsoToMain({
      type: actionTypes.DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "newtab-menu-bookmark",
    icon: "bookmark-hollow",
    action: actionCreators.AlsoToMain({
      type: actionTypes.BOOKMARK_URL,
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
    action: actionCreators.AlsoToMain({
      type: actionTypes.OPEN_NEW_WINDOW,
      data: {
        referrer: site.referrer,
        typedBonus: site.typedBonus,
        url: site.url,
        sponsored_tile_id: site.sponsored_tile_id
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
    action: actionCreators.AlsoToMain({
      type: actionTypes.BLOCK_URL,
      data: tiles.map(site => {
        var _ref;

        return {
          url: site.original_url || site.open_url || site.url,
          // pocket_id is only for pocket stories being in highlights, and then dismissed.
          pocket_id: site.pocket_id,
          // used by PlacesFeed and TopSitesFeed for sponsored top sites blocking.
          isSponsoredTopSite: site.sponsored_position,
          ...(site.flight_id ? {
            flight_id: site.flight_id
          } : {}),
          // If not sponsored, hostname could be anything (Cat3 Data!).
          // So only put in advertiser_name for sponsored topsites.
          ...(site.sponsored_position ? {
            advertiser_name: (_ref = site.label || site.hostname) === null || _ref === void 0 ? void 0 : _ref.toLocaleLowerCase()
          } : {}),
          position: pos,
          ...(site.sponsored_tile_id ? {
            tile_id: site.sponsored_tile_id
          } : {}),
          is_pocket_card: site.type === "CardGrid"
        };
      })
    }),
    impression: actionCreators.ImpressionStats({
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
    action: actionCreators.WebExtEvent(actionTypes.WEBEXT_DISMISS, {
      source: eventSource,
      url: site.url,
      action_position: index
    })
  }),
  DeleteUrl: (site, index, eventSource, isEnabled, siteInfo) => ({
    id: "newtab-menu-delete-history",
    icon: "delete",
    action: {
      type: actionTypes.DIALOG_OPEN,
      data: {
        onConfirm: [actionCreators.AlsoToMain({
          type: actionTypes.DELETE_HISTORY_URL,
          data: {
            url: site.url,
            pocket_id: site.pocket_id,
            forceBlock: site.bookmarkGuid
          }
        }), actionCreators.UserEvent(Object.assign({
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
    action: actionCreators.OnlyToMain({
      type: actionTypes.SHOW_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  OpenFile: site => ({
    id: "newtab-menu-open-file",
    icon: "open-file",
    action: actionCreators.OnlyToMain({
      type: actionTypes.OPEN_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  CopyDownloadLink: site => ({
    id: "newtab-menu-copy-download-link",
    icon: "copy",
    action: actionCreators.OnlyToMain({
      type: actionTypes.COPY_DOWNLOAD_LINK,
      data: {
        url: site.url
      }
    })
  }),
  GoToDownloadPage: site => ({
    id: "newtab-menu-go-to-download-page",
    icon: "download",
    action: actionCreators.OnlyToMain({
      type: actionTypes.OPEN_LINK,
      data: {
        url: site.referrer
      }
    }),
    disabled: !site.referrer
  }),
  RemoveDownload: site => ({
    id: "newtab-menu-remove-download",
    icon: "delete",
    action: actionCreators.OnlyToMain({
      type: actionTypes.REMOVE_DOWNLOAD_FILE,
      data: {
        url: site.url
      }
    })
  }),
  PinTopSite: (site, index) => ({
    id: "newtab-menu-pin",
    icon: "pin",
    action: actionCreators.AlsoToMain({
      type: actionTypes.TOP_SITES_PIN,
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
    action: actionCreators.AlsoToMain({
      type: actionTypes.TOP_SITES_UNPIN,
      data: {
        site: {
          url: site.url
        }
      }
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: (site, index, eventSource = "CARDGRID") => ({
    id: "newtab-menu-save-to-pocket",
    icon: "pocket-save",
    action: actionCreators.AlsoToMain({
      type: actionTypes.SAVE_TO_POCKET,
      data: {
        site: {
          url: site.url,
          title: site.title
        }
      }
    }),
    impression: actionCreators.ImpressionStats({
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
    action: actionCreators.AlsoToMain({
      type: actionTypes.DELETE_FROM_POCKET,
      data: {
        pocket_id: site.pocket_id
      }
    }),
    userEvent: "DELETE_FROM_POCKET"
  }),
  ArchiveFromPocket: site => ({
    id: "newtab-menu-archive-pocket",
    icon: "pocket-archive",
    action: actionCreators.AlsoToMain({
      type: actionTypes.ARCHIVE_FROM_POCKET,
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
      type: actionTypes.TOP_SITES_EDIT,
      data: {
        index
      }
    }
  }),
  CheckBookmark: site => site.bookmarkGuid ? LinkMenuOptions.RemoveBookmark(site) : LinkMenuOptions.AddBookmark(site),
  CheckPinTopSite: (site, index) => site.isPinned ? LinkMenuOptions.UnpinTopSite(site) : LinkMenuOptions.PinTopSite(site, index),
  CheckSavedToPocket: (site, index, source) => site.pocket_id ? LinkMenuOptions.DeleteFromPocket(site) : LinkMenuOptions.SaveToPocket(site, index, source),
  CheckBookmarkOrArchive: site => site.pocket_id ? LinkMenuOptions.ArchiveFromPocket(site) : LinkMenuOptions.CheckBookmark(site),
  CheckArchiveFromPocket: site => site.pocket_id ? LinkMenuOptions.ArchiveFromPocket(site) : LinkMenuOptions.EmptyItem(),
  CheckDeleteFromPocket: site => site.pocket_id ? LinkMenuOptions.DeleteFromPocket(site) : LinkMenuOptions.EmptyItem(),
  OpenInPrivateWindow: (site, index, eventSource, isEnabled) => isEnabled ? _OpenInPrivateWindow(site) : LinkMenuOptions.EmptyItem()
};
;// CONCATENATED MODULE: ./content-src/components/LinkMenu/LinkMenu.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];
class _LinkMenu extends (external_React_default()).PureComponent {
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
      platform,
      userEvent = actionCreators.UserEvent
    } = props; // Handle special case of default site

    const propOptions = site.isDefault && !site.searchTopSite && !site.sponsored_position ? DEFAULT_SITE_MENU_OPTIONS : props.options;
    const options = propOptions.map(o => LinkMenuOptions[o](site, index, source, isPrivateBrowsingEnabled, siteInfo, platform)).map(option => {
      const {
        action,
        impression,
        id,
        type,
        userEvent: eventName
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

          if (eventName) {
            const userEventData = Object.assign({
              event: eventName,
              source,
              action_position: index,
              value: {
                card_type: site.flight_id ? "spoc" : "organic"
              }
            }, siteInfo);
            props.dispatch(userEvent(userEventData));
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
    return /*#__PURE__*/external_React_default().createElement(ContextMenu, {
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

const LinkMenu = (0,external_ReactRedux_namespaceObject.connect)(getState)(_LinkMenu);
;// CONCATENATED MODULE: ./content-src/components/ContextMenu/ContextMenuButton.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class ContextMenuButton extends (external_React_default()).PureComponent {
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
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("button", {
      "aria-haspopup": "true",
      "data-l10n-id": tooltip,
      "data-l10n-args": tooltipArgs ? JSON.stringify(tooltipArgs) : null,
      className: "context-menu-button icon",
      onKeyDown: this.onKeyDown,
      onClick: this.onClick,
      ref: refFunction
    }), showContextMenu ? /*#__PURE__*/external_React_default().cloneElement(children, {
      keyboardAccess: contextMenuKeyboard,
      onUpdate: this.onUpdate
    }) : null);
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




class DSLinkMenu extends (external_React_default()).PureComponent {
  render() {
    const {
      index,
      dispatch
    } = this.props;
    let pocketMenuOptions = [];
    let TOP_STORIES_CONTEXT_MENU_OPTIONS = ["OpenInNewWindow", "OpenInPrivateWindow"];

    if (!this.props.isRecentSave) {
      if (this.props.pocket_button_enabled) {
        pocketMenuOptions = this.props.saveToPocketCard ? ["CheckDeleteFromPocket"] : ["CheckSavedToPocket"];
      }

      TOP_STORIES_CONTEXT_MENU_OPTIONS = ["CheckBookmark", "CheckArchiveFromPocket", ...pocketMenuOptions, "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", ...(this.props.showPrivacyInfo ? ["ShowPrivacyInfo"] : [])];
    }

    const type = this.props.type || "DISCOVERY_STREAM";
    const title = this.props.title || this.props.source;
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "context-menu-position-container"
    }, /*#__PURE__*/external_React_default().createElement(ContextMenuButton, {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.props.onMenuUpdate
    }, /*#__PURE__*/external_React_default().createElement(LinkMenu, {
      dispatch: dispatch,
      index: index,
      source: type.toUpperCase(),
      onShow: this.props.onMenuShow,
      options: TOP_STORIES_CONTEXT_MENU_OPTIONS,
      shouldSendImpressionStats: true,
      userEvent: actionCreators.DiscoveryStreamUserEvent,
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
;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSitesConstants.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const TOP_SITES_SOURCE = "TOP_SITES";
const TOP_SITES_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "DeleteUrl"];
const TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS = ["OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "ShowPrivacyInfo"];
const TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS = ["OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl", "AboutSponsored"]; // the special top site for search shortcut experiment can only have the option to unpin (which removes) the topsite

const TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS = ["CheckPinTopSite", "Separator", "BlockUrl"]; // minimum size necessary to show a rich icon instead of a screenshot

const MIN_RICH_FAVICON_SIZE = 96; // minimum size necessary to show any icon

const MIN_SMALL_FAVICON_SIZE = 16;
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamImpressionStats/ImpressionStats.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



const ImpressionStats_VISIBLE = "visible";
const ImpressionStats_VISIBILITY_CHANGE_EVENT = "visibilitychange"; // Per analytical requirement, we set the minimal intersection ratio to
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

class ImpressionStats_ImpressionStats extends (external_React_default()).PureComponent {
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: {
          flightId: this.props.flightId
        }
      })); // Record sponsored topsites impressions if the source is `TOP_SITES_SOURCE`.

      if (this.props.source === TOP_SITES_SOURCE) {
        for (const card of cards) {
          this.props.dispatch(actionCreators.OnlyToMain({
            type: actionTypes.TOP_SITES_SPONSORED_IMPRESSION_STATS,
            data: {
              type: "impression",
              tile_id: card.id,
              source: "newtab",
              advertiser: card.advertiser,
              // Keep the 0-based position, can be adjusted by the telemetry
              // sender if necessary.
              position: card.pos
            }
          }));
        }
      }
    }

    if (this._needsImpressionStats(cards)) {
      props.dispatch(actionCreators.DiscoveryStreamImpressionStats({
        source: props.source.toUpperCase(),
        window_inner_width: window.innerWidth,
        window_inner_height: window.innerHeight,
        tiles: cards.map(link => ({
          id: link.id,
          pos: link.pos,
          type: this.props.flightId ? "spoc" : "organic",
          ...(link.shim ? {
            shim: link.shim
          } : {}),
          recommendation_id: link.recommendation_id
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
      props.dispatch(actionCreators.DiscoveryStreamLoadedContent({
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

    if (props.document.visibilityState === ImpressionStats_VISIBLE) {
      // Send the loaded content ping once the page is visible.
      this._dispatchLoadedContent();

      this.setImpressionObserver();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(ImpressionStats_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      this._onVisibilityChange = () => {
        if (props.document.visibilityState === ImpressionStats_VISIBLE) {
          // Send the loaded content ping once the page is visible.
          this._dispatchLoadedContent();

          this.setImpressionObserver();
          props.document.removeEventListener(ImpressionStats_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };

      props.document.addEventListener(ImpressionStats_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
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
      this.props.document.removeEventListener(ImpressionStats_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      ref: "impression",
      className: "impression-observer"
    }, this.props.children);
  }

}
ImpressionStats_ImpressionStats.defaultProps = {
  IntersectionObserver: __webpack_require__.g.IntersectionObserver,
  document: __webpack_require__.g.document,
  rows: [],
  source: ""
};
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class SafeAnchor extends (external_React_default()).PureComponent {
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.OPEN_LINK,
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
    return /*#__PURE__*/external_React_default().createElement("a", {
      href: this.safeURI(url),
      className: className,
      onClick: this.onClick
    }, this.props.children);
  }

}
;// CONCATENATED MODULE: ./content-src/components/Card/types.js
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
;// CONCATENATED MODULE: external "ReactTransitionGroup"
const external_ReactTransitionGroup_namespaceObject = ReactTransitionGroup;
;// CONCATENATED MODULE: ./content-src/components/FluentOrText/FluentOrText.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Set text on a child element/component depending on if the message is already
 * translated plain text or a fluent id with optional args.
 */

class FluentOrText extends (external_React_default()).PureComponent {
  render() {
    // Ensure we have a single child to attach attributes
    const {
      children,
      message
    } = this.props;
    const child = children ? external_React_default().Children.only(children) : /*#__PURE__*/external_React_default().createElement("span", null); // For a string message, just use it as the child's text

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


    return /*#__PURE__*/external_React_default().cloneElement(child, extraProps, grandChildren);
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



 // Animation time is mirrored in DSContextFooter.scss

const ANIMATION_DURATION = 3000;
const DSMessageLabel = props => {
  const {
    context,
    context_type
  } = props;
  const {
    icon,
    fluentID
  } = cardContextTypes[context_type] || {};

  if (!context && context_type) {
    return /*#__PURE__*/external_React_default().createElement(external_ReactTransitionGroup_namespaceObject.TransitionGroup, {
      component: null
    }, /*#__PURE__*/external_React_default().createElement(external_ReactTransitionGroup_namespaceObject.CSSTransition, {
      key: fluentID,
      timeout: ANIMATION_DURATION,
      classNames: "story-animate"
    }, /*#__PURE__*/external_React_default().createElement(StatusMessage, {
      icon: icon,
      fluentID: fluentID
    })));
  }

  return null;
};
const StatusMessage = ({
  icon,
  fluentID
}) => /*#__PURE__*/external_React_default().createElement("div", {
  className: "status-message"
}, /*#__PURE__*/external_React_default().createElement("span", {
  "aria-haspopup": "true",
  className: `story-badge-icon icon icon-${icon}`
}), /*#__PURE__*/external_React_default().createElement("div", {
  className: "story-context-label",
  "data-l10n-id": fluentID
}));
const SponsorLabel = ({
  sponsored_by_override,
  sponsor,
  context,
  newSponsoredLabel
}) => {
  const classList = `story-sponsored-label ${newSponsoredLabel || ""} clamp`; // If override is not false or an empty string.

  if (sponsored_by_override) {
    return /*#__PURE__*/external_React_default().createElement("p", {
      className: classList
    }, sponsored_by_override);
  } else if (sponsored_by_override === "") {
    // We specifically want to display nothing if the server returns an empty string.
    // So the server can turn off the label.
    // This is to support the use cases where the sponsored context is displayed elsewhere.
    return null;
  } else if (sponsor) {
    return /*#__PURE__*/external_React_default().createElement("p", {
      className: classList
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: {
        id: `newtab-label-sponsored-by`,
        values: {
          sponsor
        }
      }
    }));
  } else if (context) {
    return /*#__PURE__*/external_React_default().createElement("p", {
      className: classList
    }, context);
  }

  return null;
};
class DSContextFooter extends (external_React_default()).PureComponent {
  render() {
    const {
      context,
      context_type,
      sponsor,
      sponsored_by_override
    } = this.props;
    const sponsorLabel = SponsorLabel({
      sponsored_by_override,
      sponsor,
      context
    });
    const dsMessageLabel = DSMessageLabel({
      context,
      context_type
    });

    if (sponsorLabel || dsMessageLabel) {
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "story-footer"
      }, sponsorLabel, dsMessageLabel);
    }

    return null;
  }

}
const DSMessageFooter = props => {
  const {
    context,
    context_type,
    saveToPocketCard
  } = props;
  const dsMessageLabel = DSMessageLabel({
    context,
    context_type
  }); // This case is specific and already displayed to the user elsewhere.

  if (!dsMessageLabel || saveToPocketCard && context_type === "pocket") {
    return null;
  }

  return /*#__PURE__*/external_React_default().createElement("div", {
    className: "story-footer"
  }, dsMessageLabel);
};
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSCard/DSCard.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */









const READING_WPM = 220;
/**
 * READ TIME FROM WORD COUNT
 * @param {int} wordCount number of words in an article
 * @returns {int} number of words per minute in minutes
 */

function readTimeFromWordCount(wordCount) {
  if (!wordCount) {
    return false;
  }

  return Math.ceil(parseInt(wordCount, 10) / READING_WPM);
}
const DSSource = ({
  source,
  timeToRead,
  newSponsoredLabel,
  context,
  sponsor,
  sponsored_by_override
}) => {
  // First try to display sponsored label or time to read here.
  if (newSponsoredLabel) {
    // If we can display something for spocs, do so.
    if (sponsored_by_override || sponsor || context) {
      return /*#__PURE__*/external_React_default().createElement(SponsorLabel, {
        context: context,
        sponsor: sponsor,
        sponsored_by_override: sponsored_by_override,
        newSponsoredLabel: "new-sponsored-label"
      });
    }
  } // If we are not a spoc, and can display a time to read value.


  if (source && timeToRead) {
    return /*#__PURE__*/external_React_default().createElement("p", {
      className: "source clamp time-to-read"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: {
        id: `newtab-label-source-read-time`,
        values: {
          source,
          timeToRead
        }
      }
    }));
  } // Otherwise display a default source.


  return /*#__PURE__*/external_React_default().createElement("p", {
    className: "source clamp"
  }, source);
};
const DefaultMeta = ({
  source,
  title,
  excerpt,
  timeToRead,
  newSponsoredLabel,
  context,
  context_type,
  sponsor,
  sponsored_by_override,
  saveToPocketCard,
  isRecentSave
}) => /*#__PURE__*/external_React_default().createElement("div", {
  className: "meta"
}, /*#__PURE__*/external_React_default().createElement("div", {
  className: "info-wrap"
}, /*#__PURE__*/external_React_default().createElement(DSSource, {
  source: source,
  timeToRead: timeToRead,
  newSponsoredLabel: newSponsoredLabel,
  context: context,
  sponsor: sponsor,
  sponsored_by_override: sponsored_by_override
}), /*#__PURE__*/external_React_default().createElement("header", {
  title: title,
  className: "title clamp"
}, title), excerpt && /*#__PURE__*/external_React_default().createElement("p", {
  className: "excerpt clamp"
}, excerpt)), !newSponsoredLabel && /*#__PURE__*/external_React_default().createElement(DSContextFooter, {
  context_type: context_type,
  context: context,
  sponsor: sponsor,
  sponsored_by_override: sponsored_by_override
}), newSponsoredLabel && /*#__PURE__*/external_React_default().createElement(DSMessageFooter, {
  context_type: context_type,
  context: null,
  saveToPocketCard: saveToPocketCard
}));
class _DSCard extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onSaveClick = this.onSaveClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);

    this.setContextMenuButtonHostRef = element => {
      this.contextMenuButtonHostElement = element;
    };

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
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: this.props.type.toUpperCase(),
        action_position: this.props.pos,
        value: {
          card_type: this.props.flightId ? "spoc" : "organic",
          recommendation_id: this.props.recommendation_id,
          tile_id: this.props.id,
          ...(this.props.shim && this.props.shim.click ? {
            shim: this.props.shim.click
          } : {})
        }
      }));
      this.props.dispatch(actionCreators.ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        window_inner_width: this.props.windowObj.innerWidth,
        window_inner_height: this.props.windowObj.innerHeight,
        tiles: [{
          id: this.props.id,
          pos: this.props.pos,
          ...(this.props.shim && this.props.shim.click ? {
            shim: this.props.shim.click
          } : {}),
          type: this.props.flightId ? "spoc" : "organic",
          recommendation_id: this.props.recommendation_id
        }]
      }));
    }
  }

  onSaveClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.SAVE_TO_POCKET,
        data: {
          site: {
            url: this.props.url,
            title: this.props.title
          }
        }
      }));
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
        event: "SAVE_TO_POCKET",
        source: "CARDGRID_HOVER",
        action_position: this.props.pos,
        value: {
          card_type: this.props.flightId ? "spoc" : "organic",
          recommendation_id: this.props.recommendation_id,
          tile_id: this.props.id,
          ...(this.props.shim && this.props.shim.save ? {
            shim: this.props.shim.save
          } : {})
        }
      }));
      this.props.dispatch(actionCreators.ImpressionStats({
        source: "CARDGRID_HOVER",
        pocket: 0,
        tiles: [{
          id: this.props.id,
          pos: this.props.pos,
          ...(this.props.shim && this.props.shim.save ? {
            shim: this.props.shim.save
          } : {}),
          recommendation_id: this.props.recommendation_id
        }]
      }));
    }
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;

      if (dsLinkMenuHostDiv) {
        dsLinkMenuHostDiv.classList.remove("active", "last-item");
      }
    }
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;

    if (dsLinkMenuHostDiv) {
      // Force translation so we can be sure it's ready before measuring.
      await this.props.windowObj.document.l10n.translateFragment(dsLinkMenuHostDiv);

      if (this.props.windowObj.scrollMaxX > 0) {
        dsLinkMenuHostDiv.classList.add("last-item");
      }

      dsLinkMenuHostDiv.classList.add("active");
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
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "ds-card placeholder",
        ref: this.setPlaceholderRef
      });
    }

    const {
      isRecentSave,
      DiscoveryStream,
      saveToPocketCard
    } = this.props;
    let source = this.props.source || this.props.publisher;

    if (!source) {
      try {
        source = new URL(this.props.url).hostname;
      } catch (e) {}
    }

    const {
      pocketButtonEnabled,
      hideDescriptions,
      compactImages,
      imageGradient,
      newSponsoredLabel,
      titleLines = 3,
      descLines = 3,
      readTime: displayReadTime
    } = DiscoveryStream;
    const excerpt = !hideDescriptions ? this.props.excerpt : "";
    let timeToRead;

    if (displayReadTime) {
      timeToRead = this.props.time_to_read || readTimeFromWordCount(this.props.word_count);
    }

    const compactImagesClassName = compactImages ? `ds-card-compact-image` : ``;
    const imageGradientClassName = imageGradient ? `ds-card-image-gradient` : ``;
    const titleLinesName = `ds-card-title-lines-${titleLines}`;
    const descLinesClassName = `ds-card-desc-lines-${descLines}`;

    let stpButton = () => {
      return /*#__PURE__*/external_React_default().createElement("button", {
        className: "card-stp-button",
        onClick: this.onSaveClick
      }, this.props.context_type === "pocket" ? /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("span", {
        className: "story-badge-icon icon icon-pocket"
      }), /*#__PURE__*/external_React_default().createElement("span", {
        "data-l10n-id": "newtab-pocket-saved"
      })) : /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("span", {
        className: "story-badge-icon icon icon-pocket-save"
      }), /*#__PURE__*/external_React_default().createElement("span", {
        "data-l10n-id": "newtab-pocket-save"
      })));
    };

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: `ds-card ${compactImagesClassName} ${imageGradientClassName} ${titleLinesName} ${descLinesClassName}`,
      ref: this.setContextMenuButtonHostRef
    }, /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      className: "ds-card-link",
      dispatch: this.props.dispatch,
      onLinkClick: !this.props.placeholder ? this.onLinkClick : undefined,
      url: this.props.url
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "img-wrapper"
    }, /*#__PURE__*/external_React_default().createElement(DSImage, {
      extraClassNames: "img",
      source: this.props.image_src,
      rawSource: this.props.raw_image_src,
      sizes: this.dsImageSizes,
      url: this.props.url,
      title: this.props.title,
      isRecentSave: isRecentSave
    })), /*#__PURE__*/external_React_default().createElement(DefaultMeta, {
      source: source,
      title: this.props.title,
      excerpt: excerpt,
      newSponsoredLabel: newSponsoredLabel,
      timeToRead: timeToRead,
      context: this.props.context,
      context_type: this.props.context_type,
      sponsor: this.props.sponsor,
      sponsored_by_override: this.props.sponsored_by_override,
      saveToPocketCard: saveToPocketCard
    }), /*#__PURE__*/external_React_default().createElement(ImpressionStats_ImpressionStats, {
      flightId: this.props.flightId,
      rows: [{
        id: this.props.id,
        pos: this.props.pos,
        ...(this.props.shim && this.props.shim.impression ? {
          shim: this.props.shim.impression
        } : {}),
        recommendation_id: this.props.recommendation_id
      }],
      dispatch: this.props.dispatch,
      source: this.props.type
    })), saveToPocketCard && /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-stp-button-hover-background"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-stp-button-position-wrapper"
    }, !this.props.flightId && stpButton(), /*#__PURE__*/external_React_default().createElement(DSLinkMenu, {
      id: this.props.id,
      index: this.props.pos,
      dispatch: this.props.dispatch,
      url: this.props.url,
      title: this.props.title,
      source: source,
      type: this.props.type,
      pocket_id: this.props.pocket_id,
      shim: this.props.shim,
      bookmarkGuid: this.props.bookmarkGuid,
      flightId: !this.props.is_collection ? this.props.flightId : undefined,
      showPrivacyInfo: !!this.props.flightId,
      onMenuUpdate: this.onMenuUpdate,
      onMenuShow: this.onMenuShow,
      saveToPocketCard: saveToPocketCard,
      pocket_button_enabled: pocketButtonEnabled,
      isRecentSave: isRecentSave
    }))), !saveToPocketCard && /*#__PURE__*/external_React_default().createElement(DSLinkMenu, {
      id: this.props.id,
      index: this.props.pos,
      dispatch: this.props.dispatch,
      url: this.props.url,
      title: this.props.title,
      source: source,
      type: this.props.type,
      pocket_id: this.props.pocket_id,
      shim: this.props.shim,
      bookmarkGuid: this.props.bookmarkGuid,
      flightId: !this.props.is_collection ? this.props.flightId : undefined,
      showPrivacyInfo: !!this.props.flightId,
      hostRef: this.contextMenuButtonHostRef,
      onMenuUpdate: this.onMenuUpdate,
      onMenuShow: this.onMenuShow,
      pocket_button_enabled: pocketButtonEnabled,
      isRecentSave: isRecentSave
    }));
  }

}
_DSCard.defaultProps = {
  windowObj: window // Added to support unit tests

};
const DSCard = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  App: state.App,
  DiscoveryStream: state.DiscoveryStream
}))(_DSCard);
const PlaceholderDSCard = props => /*#__PURE__*/external_React_default().createElement(DSCard, {
  placeholder: true
});
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class DSEmptyState extends (external_React_default()).PureComponent {
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
        type: actionTypes.DISCOVERY_STREAM_FEED_UPDATE,
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.DISCOVERY_STREAM_RETRY_FEED,
        data: {
          feed
        }
      }));
    }
  }

  renderButton() {
    if (this.props.status === "waiting" || this.state.waiting) {
      return /*#__PURE__*/external_React_default().createElement("button", {
        className: "try-again-button waiting",
        "data-l10n-id": "newtab-discovery-empty-section-topstories-loading"
      });
    }

    return /*#__PURE__*/external_React_default().createElement("button", {
      className: "try-again-button",
      onClick: this.onReset,
      "data-l10n-id": "newtab-discovery-empty-section-topstories-try-again-button"
    });
  }

  renderState() {
    if (this.props.status === "waiting" || this.props.status === "failed") {
      return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", {
        "data-l10n-id": "newtab-discovery-empty-section-topstories-timed-out"
      }), this.renderButton());
    }

    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement("h2", {
      "data-l10n-id": "newtab-discovery-empty-section-topstories-header"
    }), /*#__PURE__*/external_React_default().createElement("p", {
      "data-l10n-id": "newtab-discovery-empty-section-topstories-content"
    }));
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "section-empty-state"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "empty-state-message"
    }, this.renderState()));
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class DSDismiss extends (external_React_default()).PureComponent {
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
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: className
    }, this.props.children, /*#__PURE__*/external_React_default().createElement("button", {
      className: "ds-dismiss-button",
      "data-l10n-id": "newtab-dismiss-button-tooltip",
      onHover: this.onHover,
      onClick: this.onDismissClick,
      onMouseEnter: this.onHover,
      onMouseLeave: this.offHover
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-dismiss"
    })));
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





function _TopicsWidget(props) {
  const {
    id,
    source,
    position,
    DiscoveryStream,
    dispatch
  } = props;
  const {
    utmCampaign,
    utmContent,
    utmSource
  } = DiscoveryStream.experimentData;
  let queryParams = `?utm_source=${utmSource}`;

  if (utmCampaign && utmContent) {
    queryParams += `&utm_content=${utmContent}&utm_campaign=${utmCampaign}`;
  }

  const topics = [{
    label: "Technology",
    name: "technology"
  }, {
    label: "Science",
    name: "science"
  }, {
    label: "Self-Improvement",
    name: "self-improvement"
  }, {
    label: "Travel",
    name: "travel"
  }, {
    label: "Career",
    name: "career"
  }, {
    label: "Entertainment",
    name: "entertainment"
  }, {
    label: "Food",
    name: "food"
  }, {
    label: "Health",
    name: "health"
  }, {
    label: "Must-Reads",
    name: "must-reads",
    url: `https://getpocket.com/collections${queryParams}`
  }];

  function onLinkClick(topic, positionInCard) {
    if (dispatch) {
      dispatch(actionCreators.DiscoveryStreamUserEvent({
        event: "CLICK",
        source,
        action_position: position,
        value: {
          card_type: "topics_widget",
          topic,
          ...(positionInCard || positionInCard === 0 ? {
            position_in_card: positionInCard
          } : {})
        }
      }));
      dispatch(actionCreators.ImpressionStats({
        source,
        click: 0,
        window_inner_width: props.windowObj.innerWidth,
        window_inner_height: props.windowObj.innerHeight,
        tiles: [{
          id,
          pos: position
        }]
      }));
    }
  }

  function mapTopicItem(topic, index) {
    return /*#__PURE__*/external_React_default().createElement("li", {
      key: topic.name,
      className: topic.overflow ? "ds-topics-widget-list-overflow-item" : ""
    }, /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      url: topic.url || `https://getpocket.com/explore/${topic.name}${queryParams}`,
      dispatch: dispatch,
      onLinkClick: () => onLinkClick(topic.name, index)
    }, topic.label));
  }

  return /*#__PURE__*/external_React_default().createElement("div", {
    className: "ds-topics-widget"
  }, /*#__PURE__*/external_React_default().createElement("header", {
    className: "ds-topics-widget-header"
  }, "Popular Topics"), /*#__PURE__*/external_React_default().createElement("hr", null), /*#__PURE__*/external_React_default().createElement("div", {
    className: "ds-topics-widget-list-container"
  }, /*#__PURE__*/external_React_default().createElement("ul", null, topics.map(mapTopicItem))), /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
    className: "ds-topics-widget-button button primary",
    url: `https://getpocket.com/${queryParams}`,
    dispatch: dispatch,
    onLinkClick: () => onLinkClick("more-topics")
  }, "More Topics"), /*#__PURE__*/external_React_default().createElement(ImpressionStats_ImpressionStats, {
    dispatch: dispatch,
    rows: [{
      id,
      pos: position
    }],
    source: source
  }));
}
_TopicsWidget.defaultProps = {
  windowObj: window // Added to support unit tests

};
const TopicsWidget = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  DiscoveryStream: state.DiscoveryStream
}))(_TopicsWidget);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */









const PREF_ONBOARDING_EXPERIENCE_DISMISSED = "discoverystream.onboardingExperience.dismissed";
const CardGrid_INTERSECTION_RATIO = 0.5;
const CardGrid_VISIBLE = "visible";
const CardGrid_VISIBILITY_CHANGE_EVENT = "visibilitychange";
const WIDGET_IDS = {
  TOPICS: 1
};
function DSSubHeader({
  children
}) {
  return /*#__PURE__*/external_React_default().createElement("div", {
    className: "section-top-bar ds-sub-header"
  }, /*#__PURE__*/external_React_default().createElement("h3", {
    className: "section-title-container"
  }, children));
}
function OnboardingExperience({
  children,
  dispatch,
  windowObj = __webpack_require__.g
}) {
  const [dismissed, setDismissed] = (0,external_React_namespaceObject.useState)(false);
  const [maxHeight, setMaxHeight] = (0,external_React_namespaceObject.useState)(null);
  const heightElement = (0,external_React_namespaceObject.useRef)(null);
  const onDismissClick = (0,external_React_namespaceObject.useCallback)(() => {
    // We update this as state and redux.
    // The state update is for this newtab,
    // and the redux update is for other tabs, offscreen tabs, and future tabs.
    // We need the state update for this tab to support the transition.
    setDismissed(true);
    dispatch(actionCreators.SetPref(PREF_ONBOARDING_EXPERIENCE_DISMISSED, true));
    dispatch(actionCreators.DiscoveryStreamUserEvent({
      event: "BLOCK",
      source: "POCKET_ONBOARDING"
    }));
  }, [dispatch]);
  (0,external_React_namespaceObject.useEffect)(() => {
    const resizeObserver = new windowObj.ResizeObserver(() => {
      if (heightElement.current) {
        setMaxHeight(heightElement.current.offsetHeight);
      }
    });
    const options = {
      threshold: CardGrid_INTERSECTION_RATIO
    };
    const intersectionObserver = new windowObj.IntersectionObserver(entries => {
      if (entries.some(entry => entry.isIntersecting && entry.intersectionRatio >= CardGrid_INTERSECTION_RATIO)) {
        dispatch(actionCreators.DiscoveryStreamUserEvent({
          event: "IMPRESSION",
          source: "POCKET_ONBOARDING"
        })); // Once we have observed an impression, we can stop for this instance of newtab.

        intersectionObserver.unobserve(heightElement.current);
      }
    }, options);

    const onVisibilityChange = () => {
      intersectionObserver.observe(heightElement.current);
      windowObj.document.removeEventListener(CardGrid_VISIBILITY_CHANGE_EVENT, onVisibilityChange);
    };

    if (heightElement.current) {
      resizeObserver.observe(heightElement.current); // Check visibility or setup a visibility event to make
      // sure we don't fire this for off screen pre loaded tabs.

      if (windowObj.document.visibilityState === CardGrid_VISIBLE) {
        intersectionObserver.observe(heightElement.current);
      } else {
        windowObj.document.addEventListener(CardGrid_VISIBILITY_CHANGE_EVENT, onVisibilityChange);
      }

      setMaxHeight(heightElement.current.offsetHeight);
    } // Return unmount callback to clean up observers.


    return () => {
      resizeObserver === null || resizeObserver === void 0 ? void 0 : resizeObserver.disconnect();
      intersectionObserver === null || intersectionObserver === void 0 ? void 0 : intersectionObserver.disconnect();
      windowObj.document.removeEventListener(CardGrid_VISIBILITY_CHANGE_EVENT, onVisibilityChange);
    };
  }, [dispatch, windowObj]);
  const style = {};

  if (dismissed) {
    style.maxHeight = "0";
    style.opacity = "0";
    style.transition = "max-height 0.26s ease, opacity 0.26s ease";
  } else if (maxHeight) {
    style.maxHeight = `${maxHeight}px`;
  }

  return /*#__PURE__*/external_React_default().createElement("div", {
    style: style
  }, /*#__PURE__*/external_React_default().createElement("div", {
    className: "ds-onboarding-ref",
    ref: heightElement
  }, /*#__PURE__*/external_React_default().createElement("div", {
    className: "ds-onboarding-container"
  }, /*#__PURE__*/external_React_default().createElement(DSDismiss, {
    onDismissClick: onDismissClick,
    extraClasses: `ds-onboarding`
  }, /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("header", null, /*#__PURE__*/external_React_default().createElement("span", {
    className: "icon icon-pocket"
  }), /*#__PURE__*/external_React_default().createElement("span", {
    "data-l10n-id": "newtab-pocket-onboarding-discover"
  })), /*#__PURE__*/external_React_default().createElement("p", {
    "data-l10n-id": "newtab-pocket-onboarding-cta"
  })), /*#__PURE__*/external_React_default().createElement("div", {
    className: "ds-onboarding-graphic"
  })))));
}
function CardGrid_IntersectionObserver({
  children,
  windowObj = window,
  onIntersecting
}) {
  const intersectionElement = (0,external_React_namespaceObject.useRef)(null);
  (0,external_React_namespaceObject.useEffect)(() => {
    let observer;

    if (!observer && onIntersecting && intersectionElement.current) {
      observer = new windowObj.IntersectionObserver(entries => {
        const entry = entries.find(e => e.isIntersecting);

        if (entry) {
          // Stop observing since element has been seen
          if (observer && intersectionElement.current) {
            observer.unobserve(intersectionElement.current);
          }

          onIntersecting();
        }
      });
      observer.observe(intersectionElement.current);
    } // Cleanup


    return () => {
      var _observer;

      return (_observer = observer) === null || _observer === void 0 ? void 0 : _observer.disconnect();
    };
  }, [windowObj, onIntersecting]);
  return /*#__PURE__*/external_React_default().createElement("div", {
    ref: intersectionElement
  }, children);
}
function RecentSavesContainer({
  gridClassName = "",
  dispatch,
  windowObj = window,
  items = 3,
  source = "CARDGRID_RECENT_SAVES"
}) {
  const {
    recentSavesData,
    isUserLoggedIn,
    experimentData: {
      utmCampaign,
      utmContent,
      utmSource
    }
  } = (0,external_ReactRedux_namespaceObject.useSelector)(state => state.DiscoveryStream);
  const [visible, setVisible] = (0,external_React_namespaceObject.useState)(false);
  const onIntersecting = (0,external_React_namespaceObject.useCallback)(() => setVisible(true), []);
  (0,external_React_namespaceObject.useEffect)(() => {
    if (visible) {
      dispatch(actionCreators.AlsoToMain({
        type: actionTypes.DISCOVERY_STREAM_POCKET_STATE_INIT
      }));
    }
  }, [visible, dispatch]); // The user has not yet scrolled to this section,
  // so wait before potentially requesting Pocket data.

  if (!visible) {
    return /*#__PURE__*/external_React_default().createElement(CardGrid_IntersectionObserver, {
      windowObj: windowObj,
      onIntersecting: onIntersecting
    });
  } // Intersection observer has finished, but we're not yet logged in.


  if (visible && !isUserLoggedIn) {
    return null;
  }

  let queryParams = `?utm_source=${utmSource}`; // We really only need to add these params to urls we own.

  if (utmCampaign && utmContent) {
    queryParams += `&utm_content=${utmContent}&utm_campaign=${utmCampaign}`;
  }

  function renderCard(rec, index) {
    const url = new URL(rec.url);
    const urlSearchParams = new URLSearchParams(queryParams);

    if (rec !== null && rec !== void 0 && rec.id && !url.href.match(/getpocket\.com\/read/)) {
      url.href = `https://getpocket.com/read/${rec.id}`;
    }

    for (let [key, val] of urlSearchParams.entries()) {
      url.searchParams.set(key, val);
    }

    return /*#__PURE__*/external_React_default().createElement(DSCard, {
      key: `dscard-${(rec === null || rec === void 0 ? void 0 : rec.id) || index}`,
      id: rec.id,
      pos: index,
      type: source,
      image_src: rec.image_src,
      raw_image_src: rec.raw_image_src,
      word_count: rec.word_count,
      time_to_read: rec.time_to_read,
      title: rec.title,
      excerpt: rec.excerpt,
      url: url.href,
      source: rec.domain,
      isRecentSave: true,
      dispatch: dispatch
    });
  }

  function onMyListClicked() {
    dispatch(actionCreators.DiscoveryStreamUserEvent({
      event: "CLICK",
      source: `${source}_VIEW_LIST`
    }));
  }

  const recentSavesCards = []; // We fill the cards with a for loop over an inline map because
  // we want empty placeholders if there are not enough cards.

  for (let index = 0; index < items; index++) {
    const recentSave = recentSavesData[index];

    if (!recentSave) {
      recentSavesCards.push( /*#__PURE__*/external_React_default().createElement(PlaceholderDSCard, {
        key: `dscard-${index}`
      }));
    } else {
      var _recentSave$domain_me;

      recentSavesCards.push(renderCard({
        id: recentSave.id,
        image_src: recentSave.top_image_url,
        raw_image_src: recentSave.top_image_url,
        word_count: recentSave.word_count,
        time_to_read: recentSave.time_to_read,
        title: recentSave.resolved_title || recentSave.given_title,
        url: recentSave.resolved_url || recentSave.given_url,
        domain: (_recentSave$domain_me = recentSave.domain_metadata) === null || _recentSave$domain_me === void 0 ? void 0 : _recentSave$domain_me.name,
        excerpt: recentSave.excerpt
      }, index));
    }
  } // We are visible and logged in.


  return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement(DSSubHeader, null, /*#__PURE__*/external_React_default().createElement("span", {
    className: "section-title"
  }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
    message: "Recently Saved to your List"
  })), /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
    onLinkClick: onMyListClicked,
    className: "section-sub-link",
    url: `https://getpocket.com/a${queryParams}`
  }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
    message: "View My List"
  }))), /*#__PURE__*/external_React_default().createElement("div", {
    className: `ds-card-grid-recent-saves ${gridClassName}`
  }, recentSavesCards));
}
class _CardGrid extends (external_React_default()).PureComponent {
  renderCards() {
    var _widgets$positions, _widgets$data, _essentialReadsCards, _editorsPicksCards;

    const prefs = this.props.Prefs.values;
    const {
      items,
      hybridLayout,
      hideCardBackground,
      fourCardLayout,
      compactGrid,
      essentialReadsHeader,
      editorsPicksHeader,
      onboardingExperience,
      widgets,
      recentSavesEnabled,
      hideDescriptions,
      DiscoveryStream
    } = this.props;
    const {
      saveToPocketCard
    } = DiscoveryStream;
    const showRecentSaves = prefs.showRecentSaves && recentSavesEnabled;
    const isOnboardingExperienceDismissed = prefs[PREF_ONBOARDING_EXPERIENCE_DISMISSED];
    const recs = this.props.data.recommendations.slice(0, items);
    const cards = [];
    let essentialReadsCards = [];
    let editorsPicksCards = [];

    for (let index = 0; index < items; index++) {
      const rec = recs[index];
      cards.push(!rec || rec.placeholder ? /*#__PURE__*/external_React_default().createElement(PlaceholderDSCard, {
        key: `dscard-${index}`
      }) : /*#__PURE__*/external_React_default().createElement(DSCard, {
        key: `dscard-${rec.id}`,
        pos: rec.pos,
        flightId: rec.flight_id,
        image_src: rec.image_src,
        raw_image_src: rec.raw_image_src,
        word_count: rec.word_count,
        time_to_read: rec.time_to_read,
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
        publisher: rec.publisher,
        pocket_id: rec.pocket_id,
        context_type: rec.context_type,
        bookmarkGuid: rec.bookmarkGuid,
        is_collection: this.props.is_collection,
        saveToPocketCard: saveToPocketCard,
        recommendation_id: rec.recommendation_id
      }));
    }

    if (widgets !== null && widgets !== void 0 && (_widgets$positions = widgets.positions) !== null && _widgets$positions !== void 0 && _widgets$positions.length && widgets !== null && widgets !== void 0 && (_widgets$data = widgets.data) !== null && _widgets$data !== void 0 && _widgets$data.length) {
      let positionIndex = 0;
      const source = "CARDGRID_WIDGET";

      for (const widget of widgets.data) {
        let widgetComponent = null;
        const position = widgets.positions[positionIndex]; // Stop if we run out of positions to place widgets.

        if (!position) {
          break;
        }

        switch (widget === null || widget === void 0 ? void 0 : widget.type) {
          case "TopicsWidget":
            widgetComponent = /*#__PURE__*/external_React_default().createElement(TopicsWidget, {
              position: position.index,
              dispatch: this.props.dispatch,
              source: source,
              id: WIDGET_IDS.TOPICS
            });
            break;
        }

        if (widgetComponent) {
          // We found a widget, so up the position for next try.
          positionIndex++; // We replace an existing card with the widget.

          cards.splice(position.index, 1, widgetComponent);
        }
      }
    }

    let moreRecsHeader = ""; // For now this is English only.

    if (showRecentSaves || essentialReadsHeader && editorsPicksHeader) {
      let spliceAt = 6; // For 4 card row layouts, second row is 8 cards, and regular it is 6 cards.

      if (fourCardLayout) {
        spliceAt = 8;
      } // If we have a custom header, ensure the more recs section also has a header.


      moreRecsHeader = "More Recommendations"; // Put the first 2 rows into essentialReadsCards.

      essentialReadsCards = [...cards.splice(0, spliceAt)]; // Put the rest into editorsPicksCards.

      if (essentialReadsHeader && editorsPicksHeader) {
        editorsPicksCards = [...cards.splice(0, cards.length)];
      }
    }

    const hideCardBackgroundClass = hideCardBackground ? `ds-card-grid-hide-background` : ``;
    const fourCardLayoutClass = fourCardLayout ? `ds-card-grid-four-card-variant` : ``;
    const hideDescriptionsClassName = !hideDescriptions ? `ds-card-grid-include-descriptions` : ``;
    const compactGridClassName = compactGrid ? `ds-card-grid-compact` : ``;
    const hybridLayoutClassName = hybridLayout ? `ds-card-grid-hybrid-layout` : ``;
    const gridClassName = `ds-card-grid ${hybridLayoutClassName} ${hideCardBackgroundClass} ${fourCardLayoutClass} ${hideDescriptionsClassName} ${compactGridClassName}`;
    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, !isOnboardingExperienceDismissed && onboardingExperience && /*#__PURE__*/external_React_default().createElement(OnboardingExperience, {
      dispatch: this.props.dispatch
    }), ((_essentialReadsCards = essentialReadsCards) === null || _essentialReadsCards === void 0 ? void 0 : _essentialReadsCards.length) > 0 && /*#__PURE__*/external_React_default().createElement("div", {
      className: gridClassName
    }, essentialReadsCards), showRecentSaves && /*#__PURE__*/external_React_default().createElement(RecentSavesContainer, {
      gridClassName: gridClassName,
      dispatch: this.props.dispatch
    }), ((_editorsPicksCards = editorsPicksCards) === null || _editorsPicksCards === void 0 ? void 0 : _editorsPicksCards.length) > 0 && /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement(DSSubHeader, null, /*#__PURE__*/external_React_default().createElement("span", {
      className: "section-title"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: "Editor\u2019s Picks"
    }))), /*#__PURE__*/external_React_default().createElement("div", {
      className: gridClassName
    }, editorsPicksCards)), (cards === null || cards === void 0 ? void 0 : cards.length) > 0 && /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, moreRecsHeader && /*#__PURE__*/external_React_default().createElement(DSSubHeader, null, /*#__PURE__*/external_React_default().createElement("span", {
      className: "section-title"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: moreRecsHeader
    }))), /*#__PURE__*/external_React_default().createElement("div", {
      className: gridClassName
    }, cards)));
  }

  render() {
    const {
      data
    } = this.props; // Handle a render before feed has been fetched by displaying nothing

    if (!data) {
      return null;
    } // Handle the case where a user has dismissed all recommendations


    const isEmpty = data.recommendations.length === 0;
    return /*#__PURE__*/external_React_default().createElement("div", null, this.props.title && /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-header"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "title"
    }, this.props.title), this.props.context && /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: this.props.context
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-context"
    }))), isEmpty ? /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-card-grid empty"
    }, /*#__PURE__*/external_React_default().createElement(DSEmptyState, {
      status: data.status,
      dispatch: this.props.dispatch,
      feed: this.props.feed
    })) : this.renderCards());
  }

}
_CardGrid.defaultProps = {
  items: 4 // Number of stories to display

};
const CardGrid = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Prefs: state.Prefs,
  DiscoveryStream: state.DiscoveryStream
}))(_CardGrid);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/CollectionCardGrid/CollectionCardGrid.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





class CollectionCardGrid extends (external_React_default()).PureComponent {
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
      const blockUrlOption = LinkMenuOptions.BlockUrls(spocsData, pos, source);
      const {
        action,
        impression,
        userEvent
      } = blockUrlOption;
      this.props.dispatch(action);
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
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
      dismissible,
      pocket_button_enabled
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
    const collectionGrid = /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-collection-card-grid"
    }, /*#__PURE__*/external_React_default().createElement(CardGrid, {
      pocket_button_enabled: pocket_button_enabled,
      title: title,
      context: sponsoredByMessage,
      data: recsData,
      feed: feed,
      type: type,
      is_collection: true,
      dispatch: this.props.dispatch,
      items: this.props.items
    }));

    if (dismissible) {
      return /*#__PURE__*/external_React_default().createElement(DSDismiss, {
        onDismissClick: this.onDismissClick,
        extraClasses: `ds-dismiss-ds-collection`
      }, collectionGrid);
    }

    return collectionGrid;
  }

}
;// CONCATENATED MODULE: ./content-src/components/A11yLinkButton/A11yLinkButton.jsx
function A11yLinkButton_extends() { A11yLinkButton_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return A11yLinkButton_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function A11yLinkButton(props) {
  // function for merging classes, if necessary
  let className = "a11y-link-button";

  if (props.className) {
    className += ` ${props.className}`;
  }

  return /*#__PURE__*/external_React_default().createElement("button", A11yLinkButton_extends({
    type: "button"
  }, props, {
    className: className
  }), props.children);
}
;// CONCATENATED MODULE: ./content-src/components/ErrorBoundary/ErrorBoundary.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class ErrorBoundaryFallback extends (external_React_default()).PureComponent {
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


    return /*#__PURE__*/external_React_default().createElement("div", {
      className: className
    }, /*#__PURE__*/external_React_default().createElement("div", {
      "data-l10n-id": "newtab-error-fallback-info"
    }), /*#__PURE__*/external_React_default().createElement("span", null, /*#__PURE__*/external_React_default().createElement(A11yLinkButton, {
      className: "reload-button",
      onClick: this.onClick,
      "data-l10n-id": "newtab-error-fallback-refresh-link"
    })));
  }

}
ErrorBoundaryFallback.defaultProps = {
  className: "as-error-fallback"
};
class ErrorBoundary extends (external_React_default()).PureComponent {
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

    return /*#__PURE__*/external_React_default().createElement(this.props.FallbackComponent, {
      className: this.props.className
    });
  }

}
ErrorBoundary.defaultProps = {
  FallbackComponent: ErrorBoundaryFallback
};
;// CONCATENATED MODULE: ./content-src/components/CollapsibleSection/CollapsibleSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




/**
 * A section that can collapse. As of bug 1710937, it can no longer collapse.
 * See bug 1727365 for follow-up work to simplify this component.
 */

class _CollapsibleSection extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.onMenuButtonMouseEnter = this.onMenuButtonMouseEnter.bind(this);
    this.onMenuButtonMouseLeave = this.onMenuButtonMouseLeave.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.state = {
      menuButtonHover: false,
      showContextMenu: false
    };
    this.setContextMenuButtonRef = this.setContextMenuButtonRef.bind(this);
  }

  setContextMenuButtonRef(element) {
    this.contextMenuButtonRef = element;
  }

  onBodyMount(node) {
    this.sectionBody = node;
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
    const {
      isAnimating,
      maxHeight,
      menuButtonHover,
      showContextMenu
    } = this.state;
    const {
      id,
      collapsed,
      learnMore,
      title,
      subTitle
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

    const hasSubtitleClassName = subTitle ? `has-subtitle` : ``;
    return /*#__PURE__*/external_React_default().createElement("section", {
      className: `collapsible-section ${this.props.className}${active ? " active" : ""}` // Note: data-section-id is used for web extension api tests in mozilla central
      ,
      "data-section-id": id
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "section-top-bar"
    }, /*#__PURE__*/external_React_default().createElement("h3", {
      className: `section-title-container ${hasSubtitleClassName}`,
      style: titleStyle
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "section-title"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: title
    })), /*#__PURE__*/external_React_default().createElement("span", {
      className: "learn-more-link-wrapper"
    }, learnMore && /*#__PURE__*/external_React_default().createElement("span", {
      className: "learn-more-link"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: learnMore.link.message
    }, /*#__PURE__*/external_React_default().createElement("a", {
      href: learnMore.link.href
    })))), subTitle && /*#__PURE__*/external_React_default().createElement("span", {
      className: "section-sub-title"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: subTitle
    })))), /*#__PURE__*/external_React_default().createElement(ErrorBoundary, {
      className: "section-body-fallback"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      ref: this.onBodyMount,
      style: bodyStyle
    }, this.props.children)));
  }

}
_CollapsibleSection.defaultProps = {
  document: __webpack_require__.g.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden"
  }
};
const CollapsibleSection = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Prefs: state.Prefs
}))(_CollapsibleSection);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSMessage/DSMessage.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class DSMessage extends (external_React_default()).PureComponent {
  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-message"
    }, /*#__PURE__*/external_React_default().createElement("header", {
      className: "title"
    }, this.props.icon && /*#__PURE__*/external_React_default().createElement("div", {
      className: "glyph",
      style: {
        backgroundImage: `url(${this.props.icon})`
      }
    }), this.props.title && /*#__PURE__*/external_React_default().createElement("span", {
      className: "title-text"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: this.props.title
    })), this.props.link_text && this.props.link_url && /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      className: "link",
      url: this.props.link_url
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: this.props.link_text
    }))));
  }

}
;// CONCATENATED MODULE: ./content-src/asrouter/components/ModalOverlay/ModalOverlay.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class ModalOverlayWrapper extends (external_React_default()).PureComponent {
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

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "modalOverlayOuter active",
      onKeyDown: this.onKeyDown,
      role: "presentation"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: className,
      "aria-labelledby": props.headerId,
      id: props.id,
      role: "dialog"
    }, props.children));
  }

}
ModalOverlayWrapper.defaultProps = {
  document: __webpack_require__.g.document
};
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSPrivacyModal/DSPrivacyModal.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class DSPrivacyModal extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.onLearnLinkClick = this.onLearnLinkClick.bind(this);
    this.onManageLinkClick = this.onManageLinkClick.bind(this);
  }

  onLearnLinkClick(event) {
    this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
      event: "CLICK_PRIVACY_INFO",
      source: "DS_PRIVACY_MODAL"
    }));
  }

  onManageLinkClick(event) {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.SETTINGS_OPEN
    }));
  }

  closeModal() {
    this.props.dispatch({
      type: `HIDE_PRIVACY_INFO`,
      data: {}
    });
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement(ModalOverlayWrapper, {
      onClose: this.closeModal,
      innerClassName: "ds-privacy-modal"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "privacy-notice"
    }, /*#__PURE__*/external_React_default().createElement("h3", {
      "data-l10n-id": "newtab-privacy-modal-header"
    }), /*#__PURE__*/external_React_default().createElement("p", {
      "data-l10n-id": "newtab-privacy-modal-paragraph-2"
    }), /*#__PURE__*/external_React_default().createElement("a", {
      className: "modal-link modal-link-privacy",
      "data-l10n-id": "newtab-privacy-modal-link",
      onClick: this.onLearnLinkClick,
      href: "https://help.getpocket.com/article/1142-firefox-new-tab-recommendations-faq"
    }), /*#__PURE__*/external_React_default().createElement("button", {
      className: "modal-link modal-link-manage",
      "data-l10n-id": "newtab-privacy-modal-button-manage",
      onClick: this.onManageLinkClick
    })), /*#__PURE__*/external_React_default().createElement("section", {
      className: "actions"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: "done",
      type: "submit",
      onClick: this.closeModal,
      "data-l10n-id": "newtab-privacy-modal-button-done"
    })));
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSSignup/DSSignup.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






class DSSignup extends (external_React_default()).PureComponent {
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
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
        event: "CLICK",
        source,
        action_position: 0
      }));
      this.props.dispatch(actionCreators.ImpressionStats({
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
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: outerClassName
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-signup-content"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon icon-small-spacer icon-mail"
    }), /*#__PURE__*/external_React_default().createElement("span", null, title, " ", /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      className: "ds-chevron-link",
      dispatch: dispatch,
      onLinkClick: this.onLinkClick,
      url: url
    }, excerpt)), /*#__PURE__*/external_React_default().createElement(ImpressionStats_ImpressionStats, {
      flightId: flight_id,
      rows: [{
        id,
        pos: 0,
        shim: shim && shim.impression
      }],
      dispatch: dispatch,
      source: type
    })), /*#__PURE__*/external_React_default().createElement(ContextMenuButton, {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuButtonUpdate
    }, /*#__PURE__*/external_React_default().createElement(LinkMenu, {
      dispatch: dispatch,
      index: 0,
      source: type.toUpperCase(),
      onShow: this.onMenuShow,
      options: SIGNUP_CONTEXT_MENU_OPTIONS,
      shouldSendImpressionStats: true,
      userEvent: actionCreators.DiscoveryStreamUserEvent,
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
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/DSTextPromo/DSTextPromo.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */







class DSTextPromo extends (external_React_default()).PureComponent {
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
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
        event: "CLICK",
        source,
        action_position: 0
      }));
      this.props.dispatch(actionCreators.ImpressionStats({
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
      const blockUrlOption = LinkMenuOptions.BlockUrl(spocData, index, source);
      const {
        action,
        impression,
        userEvent
      } = blockUrlOption;
      this.props.dispatch(action);
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
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
    return /*#__PURE__*/external_React_default().createElement(DSDismiss, {
      onDismissClick: this.onDismissClick,
      extraClasses: `ds-dismiss-ds-text-promo`
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-text-promo"
    }, /*#__PURE__*/external_React_default().createElement(DSImage, {
      alt_text: alt_text,
      source: image_src,
      rawSource: raw_image_src
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "text"
    }, /*#__PURE__*/external_React_default().createElement("h3", null, `${title}\u2003`, /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      className: "ds-chevron-link",
      dispatch: this.props.dispatch,
      onLinkClick: this.onLinkClick,
      url: url
    }, cta)), /*#__PURE__*/external_React_default().createElement("p", {
      className: "subtitle"
    }, context)), /*#__PURE__*/external_React_default().createElement(ImpressionStats_ImpressionStats, {
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
;// CONCATENATED MODULE: ./content-src/lib/screenshot-utils.js
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
        url: __webpack_require__.g.URL.createObjectURL(remoteImage.data),
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
      __webpack_require__.g.URL.revokeObjectURL(localImage.url);
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
;// CONCATENATED MODULE: ./content-src/components/Card/Card.jsx
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

class _Card extends (external_React_default()).PureComponent {
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


      if (ScreenshotUtils.isRemoteImageLocal(this.state.cardImage, this.props.link.image) && !this.state.imageLoaded) {
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
    const imageInState = ScreenshotUtils.isRemoteImageLocal(prevState.cardImage, image);
    let nextState = null; // Image is updating.

    if (!imageInState && nextProps.link) {
      nextState = {
        imageLoaded: false
      };
    }

    if (imageInState) {
      return nextState;
    } // Since image was updated, attempt to revoke old image blob URL, if it exists.


    ScreenshotUtils.maybeRevokeBlobObjectURL(prevState.cardImage);
    nextState = nextState || {};
    nextState.cardImage = ScreenshotUtils.createLocalImageObject(image);
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.OPEN_DOWNLOAD_FILE,
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.OPEN_LINK,
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
      this.props.dispatch(actionCreators.WebExtEvent(actionTypes.WEBEXT_CLICK, {
        source: this.props.eventSource,
        url: this.props.link.url,
        action_position: this.props.index
      }));
    } else {
      this.props.dispatch(actionCreators.UserEvent(Object.assign({
        event: "CLICK",
        source: this.props.eventSource,
        action_position: this.props.index
      }, this._getTelemetryInfo())));

      if (this.props.shouldSendImpressionStats) {
        this.props.dispatch(actionCreators.ImpressionStats({
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
    ScreenshotUtils.maybeRevokeBlobObjectURL(this.state.cardImage);
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
    } = cardContextTypes[link.type === "now" ? "trending" : link.type] || {};
    const hasImage = this.state.cardImage || link.hasImage;
    const imageStyle = {
      backgroundImage: this.state.cardImage ? `url(${this.state.cardImage.url})` : "none"
    };
    const outerClassName = ["card-outer", className, isContextMenuOpen && "active", props.placeholder && "placeholder"].filter(v => v).join(" ");
    return /*#__PURE__*/external_React_default().createElement("li", {
      className: outerClassName
    }, /*#__PURE__*/external_React_default().createElement("a", {
      href: link.type === "pocket" ? link.open_url : link.url,
      onClick: !props.placeholder ? this.onLinkClick : undefined
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "card"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-preview-image-outer"
    }, hasImage && /*#__PURE__*/external_React_default().createElement("div", {
      className: `card-preview-image${this.state.imageLoaded ? " loaded" : ""}`,
      style: imageStyle
    })), /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-details"
    }, link.type === "download" && /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-host-name alternate",
      "data-l10n-id": "newtab-menu-open-file"
    }), link.hostname && /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-host-name"
    }, link.hostname.slice(0, 100), link.type === "download" && `  \u2014 ${link.description}`), /*#__PURE__*/external_React_default().createElement("div", {
      className: ["card-text", icon ? "" : "no-context", link.description ? "" : "no-description", link.hostname ? "" : "no-host-name"].join(" ")
    }, /*#__PURE__*/external_React_default().createElement("h4", {
      className: "card-title",
      dir: "auto"
    }, link.title), /*#__PURE__*/external_React_default().createElement("p", {
      className: "card-description",
      dir: "auto"
    }, link.description)), /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-context"
    }, icon && !link.context && /*#__PURE__*/external_React_default().createElement("span", {
      "aria-haspopup": "true",
      className: `card-context-icon icon icon-${icon}`
    }), link.icon && link.context && /*#__PURE__*/external_React_default().createElement("span", {
      "aria-haspopup": "true",
      className: "card-context-icon icon",
      style: {
        backgroundImage: `url('${link.icon}')`
      }
    }), fluentID && !link.context && /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-context-label",
      "data-l10n-id": fluentID
    }), link.context && /*#__PURE__*/external_React_default().createElement("div", {
      className: "card-context-label"
    }, link.context))))), !props.placeholder && /*#__PURE__*/external_React_default().createElement(ContextMenuButton, {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuButtonUpdate
    }, /*#__PURE__*/external_React_default().createElement(LinkMenu, {
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
const Card = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  platform: state.Prefs.values.platform
}))(_Card);
const PlaceholderCard = props => /*#__PURE__*/external_React_default().createElement(Card, {
  placeholder: true,
  className: props.className
});
;// CONCATENATED MODULE: ./content-src/lib/perf-service.js
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
;// CONCATENATED MODULE: ./content-src/components/ComponentPerfTimer/ComponentPerfTimer.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


 // Currently record only a fixed set of sections. This will prevent data
// from custom sections from showing up or from topstories.

const RECORDED_SECTIONS = ["highlights", "topsites"];
class ComponentPerfTimer extends (external_React_default()).Component {
  constructor(props) {
    super(props); // Just for test dependency injection:

    this.perfSvc = this.props.perfSvc || perfService;
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.SAVE_SESSION_PERF_DATA,
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.SAVE_SESSION_PERF_DATA,
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
;// CONCATENATED MODULE: ./content-src/components/MoreRecommendations/MoreRecommendations.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class MoreRecommendations extends (external_React_default()).PureComponent {
  render() {
    const {
      read_more_endpoint
    } = this.props;

    if (read_more_endpoint) {
      return /*#__PURE__*/external_React_default().createElement("a", {
        className: "more-recommendations",
        href: read_more_endpoint,
        "data-l10n-id": "newtab-pocket-more-recommendations"
      });
    }

    return null;
  }

}
;// CONCATENATED MODULE: ./content-src/components/PocketLoggedInCta/PocketLoggedInCta.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class _PocketLoggedInCta extends (external_React_default()).PureComponent {
  render() {
    const {
      pocketCta
    } = this.props.Pocket;
    return /*#__PURE__*/external_React_default().createElement("span", {
      className: "pocket-logged-in-cta"
    }, /*#__PURE__*/external_React_default().createElement("a", {
      className: "pocket-cta-button",
      href: pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"
    }, pocketCta.ctaButton ? pocketCta.ctaButton : /*#__PURE__*/external_React_default().createElement("span", {
      "data-l10n-id": "newtab-pocket-cta-button"
    })), /*#__PURE__*/external_React_default().createElement("a", {
      href: pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "cta-text"
    }, pocketCta.ctaText ? pocketCta.ctaText : /*#__PURE__*/external_React_default().createElement("span", {
      "data-l10n-id": "newtab-pocket-cta-text"
    }))));
  }

}
const PocketLoggedInCta = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Pocket: state.Pocket
}))(_PocketLoggedInCta);
;// CONCATENATED MODULE: ./content-src/components/Topics/Topics.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class Topic extends (external_React_default()).PureComponent {
  render() {
    const {
      url,
      name
    } = this.props;
    return /*#__PURE__*/external_React_default().createElement("li", null, /*#__PURE__*/external_React_default().createElement("a", {
      key: name,
      href: url
    }, name));
  }

}
class Topics extends (external_React_default()).PureComponent {
  render() {
    const {
      topics
    } = this.props;
    return /*#__PURE__*/external_React_default().createElement("span", {
      className: "topics"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      "data-l10n-id": "newtab-pocket-read-more"
    }), /*#__PURE__*/external_React_default().createElement("ul", null, topics && topics.map(t => /*#__PURE__*/external_React_default().createElement(Topic, {
      key: t.name,
      url: t.url,
      name: t.name
    }))));
  }

}
;// CONCATENATED MODULE: ./content-src/components/TopSites/SearchShortcutsForm.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class SelectableSearchShortcut extends (external_React_default()).PureComponent {
  render() {
    const {
      shortcut,
      selected
    } = this.props;
    const imageStyle = {
      backgroundImage: `url("${shortcut.tippyTopIcon}")`
    };
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-outer search-shortcut"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      type: "checkbox",
      id: shortcut.keyword,
      name: shortcut.keyword,
      checked: selected,
      onChange: this.props.onChange
    }), /*#__PURE__*/external_React_default().createElement("label", {
      htmlFor: shortcut.keyword
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-inner"
    }, /*#__PURE__*/external_React_default().createElement("span", null, /*#__PURE__*/external_React_default().createElement("div", {
      className: "tile"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-icon rich-icon",
      style: imageStyle,
      "data-fallback": "@"
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-icon search-topsite"
    })), /*#__PURE__*/external_React_default().createElement("div", {
      className: "title"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      dir: "auto"
    }, shortcut.keyword))))));
  }

}
class SearchShortcutsForm extends (external_React_default()).PureComponent {
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

    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.UPDATE_PINNED_SEARCH_SHORTCUTS,
      data: {
        addedShortcuts: pinQueue,
        deletedShortcuts: unpinQueue
      }
    })); // Send the Telemetry pings.

    pinQueue.forEach(shortcut => {
      this.props.dispatch(actionCreators.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "SEARCH_EDIT_ADD",
        value: {
          search_vendor: shortcut.searchVendor
        }
      }));
    });
    unpinQueue.forEach(shortcut => {
      this.props.dispatch(actionCreators.UserEvent({
        source: TOP_SITES_SOURCE,
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
    return /*#__PURE__*/external_React_default().createElement("form", {
      className: "topsite-form"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "search-shortcuts-container"
    }, /*#__PURE__*/external_React_default().createElement("h3", {
      className: "section-title grey-title",
      "data-l10n-id": "newtab-topsites-add-search-engine-header"
    }), /*#__PURE__*/external_React_default().createElement("div", null, this.state.shortcuts.map(shortcut => /*#__PURE__*/external_React_default().createElement(SelectableSearchShortcut, {
      key: shortcut.keyword,
      shortcut: shortcut,
      selected: shortcut.isSelected,
      onChange: this.handleChange
    })))), /*#__PURE__*/external_React_default().createElement("section", {
      className: "actions"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: "cancel",
      type: "button",
      onClick: this.onCancelButtonClick,
      "data-l10n-id": "newtab-topsites-cancel-button"
    }), /*#__PURE__*/external_React_default().createElement("button", {
      className: "done",
      type: "submit",
      onClick: this.onSaveButtonClick,
      "data-l10n-id": "newtab-topsites-save-button"
    })));
  }

}
;// CONCATENATED MODULE: ./common/Dedupe.sys.mjs
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

;// CONCATENATED MODULE: ./common/Reducers.sys.mjs
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */




const TOP_SITES_DEFAULT_ROWS = 1;
const TOP_SITES_MAX_SITES_PER_ROW = 8;
const PREF_COLLECTION_DISMISSIBLE = "discoverystream.isCollectionDismissible";

const dedupe = new Dedupe(site => site && site.url);

const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    locale: "",
    isForStartupCache: false,
    customizeMenuVisible: false,
  },
  ASRouter: { initialized: false },
  Snippets: { initialized: false },
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
    searchShortcuts: [],
    // The "Share-of-Voice" allocations generated by TopSitesFeed
    sov: {
      ready: false,
      positions: [
        // {position: 0, assignedPartner: "amp"},
        // {position: 1, assignedPartner: "moz-sales"},
      ],
    },
  },
  Prefs: {
    initialized: false,
    values: { featureConfig: {} },
  },
  Dialog: {
    visible: false,
    data: {},
  },
  Sections: [],
  Pocket: {
    isUserLoggedIn: null,
    pocketCta: {},
    waitingForSpoc: true,
  },
  // This is the new pocket configurable layout state.
  DiscoveryStream: {
    // This is a JSON-parsed copy of the discoverystream.config pref value.
    config: { enabled: false },
    layout: [],
    isPrivacyInfoModalVisible: false,
    isCollectionDismissible: false,
    feeds: {
      data: {
        // "https://foo.com/feed1": {lastUpdated: 123, data: [], personalized: false}
      },
      loaded: false,
    },
    spocs: {
      spocs_endpoint: "",
      lastUpdated: null,
      data: {
        // "spocs": {title: "", context: "", items: [], personalized: false},
        // "placement1": {title: "", context: "", items: [], personalized: false},
      },
      loaded: false,
      frequency_caps: [],
      blocked: [],
      placements: [],
    },
    experimentData: {
      utmSource: "pocket-newtab",
      utmCampaign: undefined,
      utmContent: undefined,
    },
    recentSavesData: [],
    isUserLoggedIn: false,
    recentSavesEnabled: false,
  },
  Personalization: {
    lastUpdated: null,
    initialized: false,
  },
  Search: {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked, we style
    // the button as if it was a focused search box and show a fake cursor but
    // really focus the awesomebar without the focus styles ("hidden focus").
    fakeFocus: false,
    // Hide the search box after handing off to AwesomeBar and user starts typing.
    hide: false,
  },
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case actionTypes.INIT:
      return Object.assign({}, prevState, action.data || {}, {
        initialized: true,
      });
    case actionTypes.TOP_SITES_UPDATED:
      // Toggle `isForStartupCache` when receiving the `TOP_SITES_UPDATE` action
      // so that sponsored tiles can be rendered as usual. See Bug 1826360.
      return Object.assign({}, prevState, action.data || {}, {
        isForStartupCache: false,
      });
    case actionTypes.SHOW_PERSONALIZE:
      return Object.assign({}, prevState, {
        customizeMenuVisible: true,
      });
    case actionTypes.HIDE_PERSONALIZE:
      return Object.assign({}, prevState, {
        customizeMenuVisible: false,
      });
    default:
      return prevState;
  }
}

function ASRouter(prevState = INITIAL_STATE.ASRouter, action) {
  switch (action.type) {
    case actionTypes.AS_ROUTER_INITIALIZED:
      return { ...action.data, initialized: true };
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
  let newLinks = links.filter(link =>
    link ? !pinnedUrls.includes(link.url) : false
  );
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
    case actionTypes.TOP_SITES_UPDATED:
      if (!action.data || !action.data.links) {
        return prevState;
      }
      return Object.assign(
        {},
        prevState,
        { initialized: true, rows: action.data.links },
        action.data.pref ? { pref: action.data.pref } : {}
      );
    case actionTypes.TOP_SITES_PREFS_UPDATED:
      return Object.assign({}, prevState, { pref: action.data.pref });
    case actionTypes.TOP_SITES_EDIT:
      return Object.assign({}, prevState, {
        editForm: {
          index: action.data.index,
          previewResponse: null,
        },
      });
    case actionTypes.TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, { editForm: null });
    case actionTypes.TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, { showSearchShortcutsForm: true });
    case actionTypes.TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, { showSearchShortcutsForm: false });
    case actionTypes.PREVIEW_RESPONSE:
      if (
        !prevState.editForm ||
        action.data.url !== prevState.editForm.previewUrl
      ) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: action.data.preview,
          previewUrl: action.data.url,
        },
      });
    case actionTypes.PREVIEW_REQUEST:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null,
          previewUrl: action.data.url,
        },
      });
    case actionTypes.PREVIEW_REQUEST_CANCEL:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null,
        },
      });
    case actionTypes.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, { screenshot: action.data.screenshot });
        }
        return row;
      });
      return hasMatch
        ? Object.assign({}, prevState, { rows: newRows })
        : prevState;
    case actionTypes.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
          return Object.assign({}, site, {
            bookmarkGuid,
            bookmarkTitle,
            bookmarkDateCreated: dateAdded,
          });
        }
        return site;
      });
      return Object.assign({}, prevState, { rows: newRows });
    case actionTypes.PLACES_BOOKMARKS_REMOVED:
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
      return Object.assign({}, prevState, { rows: newRows });
    case actionTypes.PLACES_LINKS_DELETED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.filter(
        site => !action.data.urls.includes(site.url)
      );
      return Object.assign({}, prevState, { rows: newRows });
    case actionTypes.UPDATE_SEARCH_SHORTCUTS:
      return { ...prevState, searchShortcuts: action.data.searchShortcuts };
    case actionTypes.SNIPPETS_PREVIEW_MODE:
      return { ...prevState, rows: [] };
    case actionTypes.SOV_UPDATED:
      const sov = {
        ready: action.data.ready,
        positions: action.data.positions,
      };
      return { ...prevState, sov };
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case actionTypes.DIALOG_OPEN:
      return Object.assign({}, prevState, { visible: true, data: action.data });
    case actionTypes.DIALOG_CANCEL:
      return Object.assign({}, prevState, { visible: false });
    case actionTypes.DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case actionTypes.PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, {
        initialized: true,
        values: action.data,
      });
    case actionTypes.PREF_CHANGED:
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
    case actionTypes.SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case actionTypes.SECTION_REGISTER:
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
        const initialized = !!(action.data.rows && !!action.data.rows.length);
        const section = Object.assign(
          { title: "", rows: [], enabled: false },
          action.data,
          { initialized }
        );
        newState.push(section);
      }
      return newState;
    case actionTypes.SECTION_UPDATE:
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          // If the action is updating rows, we should consider initialized to be true.
          // This can be overridden if initialized is defined in the action.data
          const initialized = action.data.rows ? { initialized: true } : {};

          // Make sure pinned cards stay at their current position when rows are updated.
          // Disabling a section (SECTION_UPDATE with empty rows) does not retain pinned cards.
          if (
            action.data.rows &&
            !!action.data.rows.length &&
            section.rows.find(card => card.pinned)
          ) {
            const rows = Array.from(action.data.rows);
            section.rows.forEach((card, index) => {
              if (card.pinned) {
                // Only add it if it's not already there.
                if (rows[index].guid !== card.guid) {
                  rows.splice(index, 0, card);
                }
              }
            });
            return Object.assign(
              {},
              section,
              initialized,
              Object.assign({}, action.data, { rows })
            );
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
            const dedupedRows = dedupeConf.dedupeFrom.reduce(
              (rows, dedupeSectionId) => {
                const dedupeSection = newState.find(
                  s => s.id === dedupeSectionId
                );
                const [, newRows] = dedupe.group(dedupeSection.rows, rows);
                return newRows;
              },
              section.rows
            );

            return Object.assign({}, section, { rows: dedupedRows });
          }

          return section;
        });
      });

      return newState;
    case actionTypes.SECTION_UPDATE_CARD:
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
    case actionTypes.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {
          rows: section.rows.map(item => {
            // find the item within the rows that is attempted to be bookmarked
            if (item.url === action.data.url) {
              const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
              return Object.assign({}, item, {
                bookmarkGuid,
                bookmarkTitle,
                bookmarkDateCreated: dateAdded,
                type: "bookmark",
              });
            }
            return item;
          }),
        })
      );
    case actionTypes.PLACES_SAVED_TO_POCKET:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {
          rows: section.rows.map(item => {
            if (item.url === action.data.url) {
              return Object.assign({}, item, {
                open_url: action.data.open_url,
                pocket_id: action.data.pocket_id,
                title: action.data.title,
                type: "pocket",
              });
            }
            return item;
          }),
        })
      );
    case actionTypes.PLACES_BOOKMARKS_REMOVED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {
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
          }),
        })
      );
    case actionTypes.PLACES_LINKS_DELETED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {
          rows: section.rows.filter(
            site => !action.data.urls.includes(site.url)
          ),
        })
      );
    case actionTypes.PLACES_LINK_BLOCKED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {
          rows: section.rows.filter(site => site.url !== action.data.url),
        })
      );
    case actionTypes.DELETE_FROM_POCKET:
    case actionTypes.ARCHIVE_FROM_POCKET:
      return prevState.map(section =>
        Object.assign({}, section, {
          rows: section.rows.filter(
            site => site.pocket_id !== action.data.pocket_id
          ),
        })
      );
    case actionTypes.SNIPPETS_PREVIEW_MODE:
      return prevState.map(section => ({ ...section, rows: [] }));
    default:
      return prevState;
  }
}

function Snippets(prevState = INITIAL_STATE.Snippets, action) {
  switch (action.type) {
    case actionTypes.SNIPPETS_DATA:
      return Object.assign({}, prevState, { initialized: true }, action.data);
    case actionTypes.SNIPPET_BLOCKED:
      return Object.assign({}, prevState, {
        blockList: prevState.blockList.concat(action.data),
      });
    case actionTypes.SNIPPETS_BLOCKLIST_CLEARED:
      return Object.assign({}, prevState, { blockList: [] });
    case actionTypes.SNIPPETS_RESET:
      return INITIAL_STATE.Snippets;
    default:
      return prevState;
  }
}

function Pocket(prevState = INITIAL_STATE.Pocket, action) {
  switch (action.type) {
    case actionTypes.POCKET_WAITING_FOR_SPOC:
      return { ...prevState, waitingForSpoc: action.data };
    case actionTypes.POCKET_LOGGED_IN:
      return { ...prevState, isUserLoggedIn: !!action.data };
    case actionTypes.POCKET_CTA:
      return {
        ...prevState,
        pocketCta: {
          ctaButton: action.data.cta_button,
          ctaText: action.data.cta_text,
          ctaUrl: action.data.cta_url,
          useCta: action.data.use_cta,
        },
      };
    default:
      return prevState;
  }
}

function Reducers_sys_Personalization(prevState = INITIAL_STATE.Personalization, action) {
  switch (action.type) {
    case actionTypes.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED:
      return {
        ...prevState,
        lastUpdated: action.data.lastUpdated,
      };
    case actionTypes.DISCOVERY_STREAM_PERSONALIZATION_INIT:
      return {
        ...prevState,
        initialized: true,
      };
    case actionTypes.DISCOVERY_STREAM_PERSONALIZATION_RESET:
      return { ...INITIAL_STATE.Personalization };
    default:
      return prevState;
  }
}

// eslint-disable-next-line complexity
function DiscoveryStream(prevState = INITIAL_STATE.DiscoveryStream, action) {
  // Return if action data is empty, or spocs or feeds data is not loaded
  const isNotReady = () =>
    !action.data || !prevState.spocs.loaded || !prevState.feeds.loaded;

  const handlePlacements = handleSites => {
    const { data, placements } = prevState.spocs;
    const result = {};

    const forPlacement = placement => {
      const placementSpocs = data[placement.name];

      if (
        !placementSpocs ||
        !placementSpocs.items ||
        !placementSpocs.items.length
      ) {
        return;
      }

      result[placement.name] = {
        ...placementSpocs,
        items: handleSites(placementSpocs.items),
      };
    };

    if (!placements || !placements.length) {
      [{ name: "spocs" }].forEach(forPlacement);
    } else {
      placements.forEach(forPlacement);
    }
    return result;
  };

  const nextState = handleSites => ({
    ...prevState,
    spocs: {
      ...prevState.spocs,
      data: handlePlacements(handleSites),
    },
    feeds: {
      ...prevState.feeds,
      data: Object.keys(prevState.feeds.data).reduce(
        (accumulator, feed_url) => {
          accumulator[feed_url] = {
            data: {
              ...prevState.feeds.data[feed_url].data,
              recommendations: handleSites(
                prevState.feeds.data[feed_url].data.recommendations
              ),
            },
          };
          return accumulator;
        },
        {}
      ),
    },
  });

  switch (action.type) {
    case actionTypes.DISCOVERY_STREAM_CONFIG_CHANGE:
    // Fall through to a separate action is so it doesn't trigger a listener update on init
    case actionTypes.DISCOVERY_STREAM_CONFIG_SETUP:
      return { ...prevState, config: action.data || {} };
    case actionTypes.DISCOVERY_STREAM_EXPERIMENT_DATA:
      return { ...prevState, experimentData: action.data || {} };
    case actionTypes.DISCOVERY_STREAM_LAYOUT_UPDATE:
      return {
        ...prevState,
        layout: action.data.layout || [],
      };
    case actionTypes.DISCOVERY_STREAM_COLLECTION_DISMISSIBLE_TOGGLE:
      return {
        ...prevState,
        isCollectionDismissible: action.data.value,
      };
    case actionTypes.DISCOVERY_STREAM_PREFS_SETUP:
      return {
        ...prevState,
        recentSavesEnabled: action.data.recentSavesEnabled,
        pocketButtonEnabled: action.data.pocketButtonEnabled,
        saveToPocketCard: action.data.saveToPocketCard,
        hideDescriptions: action.data.hideDescriptions,
        compactImages: action.data.compactImages,
        imageGradient: action.data.imageGradient,
        newSponsoredLabel: action.data.newSponsoredLabel,
        titleLines: action.data.titleLines,
        descLines: action.data.descLines,
        readTime: action.data.readTime,
      };
    case actionTypes.DISCOVERY_STREAM_RECENT_SAVES:
      return {
        ...prevState,
        recentSavesData: action.data.recentSaves,
      };
    case actionTypes.DISCOVERY_STREAM_POCKET_STATE_SET:
      return {
        ...prevState,
        isUserLoggedIn: action.data.isUserLoggedIn,
      };
    case actionTypes.HIDE_PRIVACY_INFO:
      return {
        ...prevState,
        isPrivacyInfoModalVisible: false,
      };
    case actionTypes.SHOW_PRIVACY_INFO:
      return {
        ...prevState,
        isPrivacyInfoModalVisible: true,
      };
    case actionTypes.DISCOVERY_STREAM_LAYOUT_RESET:
      return { ...INITIAL_STATE.DiscoveryStream, config: prevState.config };
    case actionTypes.DISCOVERY_STREAM_FEEDS_UPDATE:
      return {
        ...prevState,
        feeds: {
          ...prevState.feeds,
          loaded: true,
        },
      };
    case actionTypes.DISCOVERY_STREAM_FEED_UPDATE:
      const newData = {};
      newData[action.data.url] = action.data.feed;
      return {
        ...prevState,
        feeds: {
          ...prevState.feeds,
          data: {
            ...prevState.feeds.data,
            ...newData,
          },
        },
      };
    case actionTypes.DISCOVERY_STREAM_SPOCS_CAPS:
      return {
        ...prevState,
        spocs: {
          ...prevState.spocs,
          frequency_caps: [...prevState.spocs.frequency_caps, ...action.data],
        },
      };
    case actionTypes.DISCOVERY_STREAM_SPOCS_ENDPOINT:
      return {
        ...prevState,
        spocs: {
          ...INITIAL_STATE.DiscoveryStream.spocs,
          spocs_endpoint:
            action.data.url ||
            INITIAL_STATE.DiscoveryStream.spocs.spocs_endpoint,
        },
      };
    case actionTypes.DISCOVERY_STREAM_SPOCS_PLACEMENTS:
      return {
        ...prevState,
        spocs: {
          ...prevState.spocs,
          placements:
            action.data.placements ||
            INITIAL_STATE.DiscoveryStream.spocs.placements,
        },
      };
    case actionTypes.DISCOVERY_STREAM_SPOCS_UPDATE:
      if (action.data) {
        return {
          ...prevState,
          spocs: {
            ...prevState.spocs,
            lastUpdated: action.data.lastUpdated,
            data: action.data.spocs,
            loaded: true,
          },
        };
      }
      return prevState;
    case actionTypes.DISCOVERY_STREAM_SPOC_BLOCKED:
      return {
        ...prevState,
        spocs: {
          ...prevState.spocs,
          blocked: [...prevState.spocs.blocked, action.data.url],
        },
      };
    case actionTypes.DISCOVERY_STREAM_LINK_BLOCKED:
      return isNotReady()
        ? prevState
        : nextState(items =>
            items.filter(item => item.url !== action.data.url)
          );

    case actionTypes.PLACES_SAVED_TO_POCKET:
      const addPocketInfo = item => {
        if (item.url === action.data.url) {
          return Object.assign({}, item, {
            open_url: action.data.open_url,
            pocket_id: action.data.pocket_id,
            context_type: "pocket",
          });
        }
        return item;
      };
      return isNotReady()
        ? prevState
        : nextState(items => items.map(addPocketInfo));

    case actionTypes.DELETE_FROM_POCKET:
    case actionTypes.ARCHIVE_FROM_POCKET:
      return isNotReady()
        ? prevState
        : nextState(items =>
            items.filter(item => item.pocket_id !== action.data.pocket_id)
          );

    case actionTypes.PLACES_BOOKMARK_ADDED:
      const updateBookmarkInfo = item => {
        if (item.url === action.data.url) {
          const { bookmarkGuid, bookmarkTitle, dateAdded } = action.data;
          return Object.assign({}, item, {
            bookmarkGuid,
            bookmarkTitle,
            bookmarkDateCreated: dateAdded,
            context_type: "bookmark",
          });
        }
        return item;
      };
      return isNotReady()
        ? prevState
        : nextState(items => items.map(updateBookmarkInfo));

    case actionTypes.PLACES_BOOKMARKS_REMOVED:
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
      return isNotReady()
        ? prevState
        : nextState(items => items.map(removeBookmarkInfo));
    case actionTypes.PREF_CHANGED:
      if (action.data.name === PREF_COLLECTION_DISMISSIBLE) {
        return {
          ...prevState,
          isCollectionDismissible: action.data.value,
        };
      }
      return prevState;
    default:
      return prevState;
  }
}

function Search(prevState = INITIAL_STATE.Search, action) {
  switch (action.type) {
    case actionTypes.DISABLE_SEARCH:
      return Object.assign({ ...prevState, disable: true });
    case actionTypes.FAKE_FOCUS_SEARCH:
      return Object.assign({ ...prevState, fakeFocus: true });
    case actionTypes.SHOW_SEARCH:
      return Object.assign({ ...prevState, disable: false, fakeFocus: false });
    default:
      return prevState;
  }
}

const reducers = {
  TopSites,
  App,
  ASRouter,
  Snippets,
  Prefs,
  Dialog,
  Sections,
  Pocket,
  Personalization: Reducers_sys_Personalization,
  DiscoveryStream,
  Search,
};

;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteFormInput.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class TopSiteFormInput extends (external_React_default()).PureComponent {
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
      return /*#__PURE__*/external_React_default().createElement("div", {
        className: "loading-container"
      }, /*#__PURE__*/external_React_default().createElement("div", {
        className: "loading-animation"
      }));
    } else if (showClearButton) {
      return /*#__PURE__*/external_React_default().createElement("button", {
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
    return /*#__PURE__*/external_React_default().createElement("label", null, /*#__PURE__*/external_React_default().createElement("span", {
      "data-l10n-id": this.props.titleId
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: `field ${typeUrl ? "url" : ""}${validationError ? " invalid" : ""}`
    }, /*#__PURE__*/external_React_default().createElement("input", {
      type: "text",
      value: this.props.value,
      ref: this.onMount,
      onChange: this.onChange,
      "data-l10n-id": this.props.placeholderId // Set focus on error if the url field is valid or when the input is first rendered and is empty
      // eslint-disable-next-line jsx-a11y/no-autofocus
      ,
      autoFocus: this.props.autoFocusOnOpen,
      disabled: this.props.loading
    }), this.renderLoadingOrCloseButton(), validationError && /*#__PURE__*/external_React_default().createElement("aside", {
      className: "error-tooltip",
      "data-l10n-id": this.props.errorMessageId
    })));
  }

}
TopSiteFormInput.defaultProps = {
  showClearButton: false,
  value: "",
  validationError: false
};
;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteImpressionWrapper.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const TopSiteImpressionWrapper_VISIBLE = "visible";
const TopSiteImpressionWrapper_VISIBILITY_CHANGE_EVENT = "visibilitychange"; // Per analytical requirement, we set the minimal intersection ratio to
// 0.5, and an impression is identified when the wrapped item has at least
// 50% visibility.
//
// This constant is exported for unit test

const TopSiteImpressionWrapper_INTERSECTION_RATIO = 0.5;
/**
 * Impression wrapper for a TopSite tile.
 *
 * It makses use of the Intersection Observer API to detect the visibility,
 * and relies on page visibility to ensure the impression is reported
 * only when the component is visible on the page.
 */

class TopSiteImpressionWrapper extends (external_React_default()).PureComponent {
  _dispatchImpressionStats() {
    const {
      actionType,
      tile
    } = this.props;

    if (!actionType) {
      return;
    }

    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionType,
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

    if (props.document.visibilityState === TopSiteImpressionWrapper_VISIBLE) {
      this.setImpressionObserver();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(TopSiteImpressionWrapper_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      }

      this._onVisibilityChange = () => {
        if (props.document.visibilityState === TopSiteImpressionWrapper_VISIBLE) {
          this.setImpressionObserver();
          props.document.removeEventListener(TopSiteImpressionWrapper_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };

      props.document.addEventListener(TopSiteImpressionWrapper_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
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
      if (entries.some(entry => entry.isIntersecting && entry.intersectionRatio >= TopSiteImpressionWrapper_INTERSECTION_RATIO)) {
        this._dispatchImpressionStats();

        this.impressionObserver.unobserve(this.refs.topsite_impression_wrapper);
      }
    };

    const options = {
      threshold: TopSiteImpressionWrapper_INTERSECTION_RATIO
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
      this.props.document.removeEventListener(TopSiteImpressionWrapper_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      ref: "topsite_impression_wrapper",
      className: "topsite-impression-observer"
    }, this.props.children);
  }

}
TopSiteImpressionWrapper.defaultProps = {
  IntersectionObserver: __webpack_require__.g.IntersectionObserver,
  document: __webpack_require__.g.document,
  actionType: null,
  tile: null
};
;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSite.jsx
function TopSite_extends() { TopSite_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return TopSite_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */










const SPOC_TYPE = "SPOC";
const NEWTAB_SOURCE = "newtab"; // For cases if we want to know if this is sponsored by either sponsored_position or type.
// We have two sources for sponsored topsites, and
// sponsored_position is set by one sponsored source, and type is set by another.
// This is not called in all cases, sometimes we want to know if it's one source
// or the other. This function is only applicable in cases where we only care if it's either.

function isSponsored(link) {
  return (link === null || link === void 0 ? void 0 : link.sponsored_position) || (link === null || link === void 0 ? void 0 : link.type) === SPOC_TYPE;
}

class TopSiteLink extends (external_React_default()).PureComponent {
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
    return (this.dragged || !isSponsored(this.props.link)) && e.dataTransfer.types.includes("text/topsite-index");
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

        if (isSponsored(this.props.link)) {
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
    const imageInState = ScreenshotUtils.isRemoteImageLocal(prevState.screenshotImage, screenshot);

    if (imageInState) {
      return null;
    } // Since image was updated, attempt to revoke old image blob URL, if it exists.


    ScreenshotUtils.maybeRevokeBlobObjectURL(prevState.screenshotImage);
    return {
      screenshotImage: ScreenshotUtils.createLocalImageObject(screenshot)
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
    ScreenshotUtils.maybeRevokeBlobObjectURL(this.state.screenshotImage);
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
      link
    } = this.props;
    const {
      tippyTopIcon,
      faviconSize
    } = link;
    let imageClassName;
    let imageStyle;
    let showSmallFavicon = false;
    let smallFaviconStyle;
    let hasScreenshotImage = this.state.screenshotImage && this.state.screenshotImage.url;
    let selectedColor;

    if (defaultStyle) {
      // force no styles (letter fallback) even if the link has imagery
      selectedColor = this.generateColor();
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
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: hasScreenshotImage ? `url(${this.state.screenshotImage.url})` : ""
      };
    } else if (tippyTopIcon || link.type === SPOC_TYPE || faviconSize >= MIN_RICH_FAVICON_SIZE) {
      // styles and class names for top sites with rich icons
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon || link.favicon})`
      };
    } else if (faviconSize >= MIN_SMALL_FAVICON_SIZE) {
      showSmallFavicon = true;
      smallFaviconStyle = {
        backgroundImage: `url(${link.favicon})`
      };
    } else {
      selectedColor = this.generateColor();
      imageClassName = "";
    }

    return {
      showSmallFavicon,
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
      title
    } = this.props;
    const topSiteOuterClassName = `top-site-outer${className ? ` ${className}` : ""}${link.isDragged ? " dragged" : ""}${link.searchTopSite ? " search-shortcut" : ""}`;
    const [letterFallback] = title;
    const {
      showSmallFavicon,
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

    let impressionStats = null;

    if (link.type === SPOC_TYPE) {
      // Record impressions for Pocket tiles.
      impressionStats = /*#__PURE__*/external_React_default().createElement(ImpressionStats_ImpressionStats, {
        flightId: link.flightId,
        rows: [{
          id: link.id,
          pos: link.pos,
          shim: link.shim && link.shim.impression,
          advertiser: title.toLocaleLowerCase()
        }],
        dispatch: this.props.dispatch,
        source: TOP_SITES_SOURCE
      });
    } else if (isSponsored(link)) {
      // Record impressions for non-Pocket sponsored tiles.
      impressionStats = /*#__PURE__*/external_React_default().createElement(TopSiteImpressionWrapper, {
        actionType: actionTypes.TOP_SITES_SPONSORED_IMPRESSION_STATS,
        tile: {
          position: this.props.index,
          tile_id: link.sponsored_tile_id || -1,
          reporting_url: link.sponsored_impression_url,
          advertiser: title.toLocaleLowerCase(),
          source: NEWTAB_SOURCE
        } // For testing.
        ,
        IntersectionObserver: this.props.IntersectionObserver,
        document: this.props.document,
        dispatch: this.props.dispatch
      });
    } else {
      // Record impressions for organic tiles.
      impressionStats = /*#__PURE__*/external_React_default().createElement(TopSiteImpressionWrapper, {
        actionType: actionTypes.TOP_SITES_ORGANIC_IMPRESSION_STATS,
        tile: {
          position: this.props.index,
          source: NEWTAB_SOURCE
        } // For testing.
        ,
        IntersectionObserver: this.props.IntersectionObserver,
        document: this.props.document,
        dispatch: this.props.dispatch
      });
    }

    return /*#__PURE__*/external_React_default().createElement("li", TopSite_extends({
      className: topSiteOuterClassName,
      onDrop: this.onDragEvent,
      onDragOver: this.onDragEvent,
      onDragEnter: this.onDragEvent,
      onDragLeave: this.onDragEvent
    }, draggableProps), /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-inner"
    }, /*#__PURE__*/external_React_default().createElement("a", {
      className: "top-site-button",
      href: link.searchTopSite ? undefined : link.url,
      tabIndex: "0",
      onKeyPress: this.onKeyPress,
      onClick: onClick,
      draggable: true,
      "data-is-sponsored-link": !!link.sponsored_tile_id
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "tile",
      "aria-hidden": true
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: selectedColor ? "icon-wrapper letter-fallback" : "icon-wrapper",
      "data-fallback": letterFallback,
      style: selectedColor ? {
        backgroundColor: selectedColor
      } : {}
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: imageClassName,
      style: imageStyle
    }), showSmallFavicon && /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-icon default-icon",
      "data-fallback": smallFaviconStyle ? "" : letterFallback,
      style: smallFaviconStyle
    })), link.searchTopSite && /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-site-icon search-topsite"
    })), /*#__PURE__*/external_React_default().createElement("div", {
      className: `title${link.isPinned ? " has-icon pinned" : ""}${link.type === SPOC_TYPE || link.show_sponsored_label ? " sponsored" : ""}`
    }, /*#__PURE__*/external_React_default().createElement("span", {
      dir: "auto"
    }, link.isPinned && /*#__PURE__*/external_React_default().createElement("div", {
      className: "icon icon-pin-small"
    }), title || /*#__PURE__*/external_React_default().createElement("br", null), /*#__PURE__*/external_React_default().createElement("span", {
      className: "sponsored-label",
      "data-l10n-id": "newtab-topsite-sponsored"
    })))), children, impressionStats));
  }

}
TopSiteLink.defaultProps = {
  title: "",
  link: {},
  isDraggable: true
};
class TopSite extends (external_React_default()).PureComponent {
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

    if (isSponsored(this.props.link)) {
      value.card_type = "spoc";
    }

    return {
      value
    };
  }

  userEvent(event) {
    this.props.dispatch(actionCreators.UserEvent(Object.assign({
      event,
      source: TOP_SITES_SOURCE,
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
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.OPEN_LINK,
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

      if (this.props.link.type === SPOC_TYPE) {
        // Record a Pocket-specific click.
        this.props.dispatch(actionCreators.ImpressionStats({
          source: TOP_SITES_SOURCE,
          click: 0,
          tiles: [{
            id: this.props.link.id,
            pos: this.props.link.pos,
            shim: this.props.link.shim && this.props.link.shim.click
          }]
        })); // Record a click for a Pocket sponsored tile.

        const title = this.props.link.label || this.props.link.hostname;
        this.props.dispatch(actionCreators.OnlyToMain({
          type: actionTypes.TOP_SITES_SPONSORED_IMPRESSION_STATS,
          data: {
            type: "click",
            position: this.props.link.pos,
            tile_id: this.props.link.id,
            advertiser: title.toLocaleLowerCase(),
            source: NEWTAB_SOURCE
          }
        }));
      } else if (isSponsored(this.props.link)) {
        // Record a click for a non-Pocket sponsored tile.
        const title = this.props.link.label || this.props.link.hostname;
        this.props.dispatch(actionCreators.OnlyToMain({
          type: actionTypes.TOP_SITES_SPONSORED_IMPRESSION_STATS,
          data: {
            type: "click",
            position: this.props.index,
            tile_id: this.props.link.sponsored_tile_id || -1,
            reporting_url: this.props.link.sponsored_click_url,
            advertiser: title.toLocaleLowerCase(),
            source: NEWTAB_SOURCE
          }
        }));
      } else {
        // Record a click for an organic tile.
        this.props.dispatch(actionCreators.OnlyToMain({
          type: actionTypes.TOP_SITES_ORGANIC_IMPRESSION_STATS,
          data: {
            type: "click",
            position: this.props.index,
            source: NEWTAB_SOURCE
          }
        }));
      }

      if (this.props.link.sendAttributionRequest) {
        this.props.dispatch(actionCreators.OnlyToMain({
          type: actionTypes.PARTNER_LINK_ATTRIBUTION,
          data: {
            targetURL: this.props.link.url,
            source: "newtab"
          }
        }));
      }
    } else {
      this.props.dispatch(actionCreators.OnlyToMain({
        type: actionTypes.FILL_SEARCH_TERM,
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
      menuOptions = TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS;
    } else if (link.searchTopSite) {
      menuOptions = TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS;
    } else if (link.type === SPOC_TYPE) {
      menuOptions = TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS;
    } else {
      menuOptions = TOP_SITES_CONTEXT_MENU_OPTIONS;
    }

    return /*#__PURE__*/external_React_default().createElement(TopSiteLink, TopSite_extends({}, props, {
      onClick: this.onLinkClick,
      onDragEvent: this.props.onDragEvent,
      className: `${props.className || ""}${isContextMenuOpen ? " active" : ""}`,
      title: title
    }), /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement(ContextMenuButton, {
      tooltip: "newtab-menu-content-tooltip",
      tooltipArgs: {
        title
      },
      onUpdate: this.onMenuUpdate
    }, /*#__PURE__*/external_React_default().createElement(LinkMenu, {
      dispatch: props.dispatch,
      index: props.index,
      onUpdate: this.onMenuUpdate,
      options: menuOptions,
      site: link,
      shouldSendImpressionStats: link.type === SPOC_TYPE,
      siteInfo: this._getTelemetryInfo(),
      source: TOP_SITES_SOURCE
    }))));
  }

}
TopSite.defaultProps = {
  link: {},

  onActivate() {}

};
class TopSitePlaceholder extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
  }

  onEditButtonClick() {
    this.props.dispatch({
      type: actionTypes.TOP_SITES_EDIT,
      data: {
        index: this.props.index
      }
    });
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement(TopSiteLink, TopSite_extends({}, this.props, {
      className: `placeholder ${this.props.className || ""}`,
      isDraggable: false
    }), /*#__PURE__*/external_React_default().createElement("button", {
      "aria-haspopup": "dialog",
      className: "context-menu-button edit-button icon",
      "data-l10n-id": "newtab-menu-topsites-placeholder-tooltip",
      onClick: this.onEditButtonClick
    }));
  }

}
class _TopSiteList extends (external_React_default()).PureComponent {
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
    this.props.dispatch(actionCreators.UserEvent({
      event,
      source: TOP_SITES_SOURCE,
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
          this.props.dispatch(actionCreators.AlsoToMain({
            type: actionTypes.TOP_SITES_INSERT,
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
    topSites.length = this.props.TopSitesRows * TOP_SITES_MAX_SITES_PER_ROW;
    return topSites;
  }
  /**
   * Make a preview of the topsites that will be the result of dropping the currently
   * dragged site at the specified index.
   */


  _makeTopSitesPreview(index) {
    const topSites = this._getTopSites();

    topSites[this.state.draggedIndex] = null;
    const preview = topSites.map(site => site && (site.isPinned || isSponsored(site)) ? site : null);
    const unpinned = topSites.filter(site => site && !site.isPinned && !isSponsored(site));
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

        while (isSponsored(preview[nextIndex])) {
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
      dispatch: props.dispatch
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

      let topSiteLink; // Use a placeholder if the link is empty or it's rendering a sponsored
      // tile for the about:home startup cache.

      if (!link || props.App.isForStartupCache && isSponsored(link)) {
        topSiteLink = /*#__PURE__*/external_React_default().createElement(TopSitePlaceholder, TopSite_extends({}, slotProps, commonProps));
      } else {
        topSiteLink = /*#__PURE__*/external_React_default().createElement(TopSite, TopSite_extends({
          link: link,
          activeIndex: this.state.activeIndex,
          onActivate: this.onActivate
        }, slotProps, commonProps, {
          colors: props.colors
        }));
      }

      topSitesUI.push(topSiteLink);
    }

    return /*#__PURE__*/external_React_default().createElement("ul", {
      className: `top-sites-list${this.state.draggedSite ? " dnd-active" : ""}`
    }, topSitesUI);
  }

}
const TopSiteList = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  App: state.App
}))(_TopSiteList);
;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSiteForm.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






class TopSiteForm extends (external_React_default()).PureComponent {
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
      type: actionTypes.PREVIEW_REQUEST_CANCEL
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

      this.props.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.TOP_SITES_PIN,
        data: {
          site,
          index
        }
      }));
      this.props.dispatch(actionCreators.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "TOP_SITES_EDIT",
        action_position: index
      }));
      this.props.onClose();
    }
  }

  onPreviewButtonClick(event) {
    event.preventDefault();

    if (this.validateForm()) {
      this.props.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.PREVIEW_REQUEST,
        data: {
          url: this.cleanUrl(this.state.customScreenshotUrl)
        }
      }));
      this.props.dispatch(actionCreators.UserEvent({
        source: TOP_SITES_SOURCE,
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
      return /*#__PURE__*/external_React_default().createElement(A11yLinkButton, {
        onClick: this.onEnableScreenshotUrlForm,
        className: "enable-custom-image-input",
        "data-l10n-id": "newtab-topsites-use-image-link"
      });
    }

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "custom-image-input-container"
    }, /*#__PURE__*/external_React_default().createElement(TopSiteFormInput, {
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


    const onSubmit = previewMode ? this.onPreviewButtonClick : this.onDoneButtonClick;
    const addTopsitesHeaderL10nId = "newtab-topsites-add-shortcut-header";
    const editTopsitesHeaderL10nId = "newtab-topsites-edit-shortcut-header";
    return /*#__PURE__*/external_React_default().createElement("form", {
      className: "topsite-form",
      onSubmit: onSubmit
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "form-input-container"
    }, /*#__PURE__*/external_React_default().createElement("h3", {
      className: "section-title grey-title",
      "data-l10n-id": showAsAdd ? addTopsitesHeaderL10nId : editTopsitesHeaderL10nId
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "fields-and-preview"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "form-wrapper"
    }, /*#__PURE__*/external_React_default().createElement(TopSiteFormInput, {
      onChange: this.onLabelChange,
      value: this.state.label,
      titleId: "newtab-topsites-title-label",
      placeholderId: "newtab-topsites-title-input",
      autoFocusOnOpen: true
    }), /*#__PURE__*/external_React_default().createElement(TopSiteFormInput, {
      onChange: this.onUrlChange,
      shouldFocus: this.state.validationError && !this.validateUrl(this.state.url),
      value: this.state.url,
      onClear: this.onClearUrlClick,
      validationError: this.state.validationError && !this.validateUrl(this.state.url),
      titleId: "newtab-topsites-url-label",
      typeUrl: true,
      placeholderId: "newtab-topsites-url-input",
      errorMessageId: "newtab-topsites-url-validation"
    }), this._renderCustomScreenshotInput()), /*#__PURE__*/external_React_default().createElement(TopSiteLink, {
      link: previewLink,
      defaultStyle: requestFailed,
      title: this.state.label
    }))), /*#__PURE__*/external_React_default().createElement("section", {
      className: "actions"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: "cancel",
      type: "button",
      onClick: this.onCancelButtonClick,
      "data-l10n-id": "newtab-topsites-cancel-button"
    }), previewMode ? /*#__PURE__*/external_React_default().createElement("button", {
      className: "done preview",
      type: "submit",
      "data-l10n-id": "newtab-topsites-preview-button"
    }) : /*#__PURE__*/external_React_default().createElement("button", {
      className: "done",
      type: "submit",
      "data-l10n-id": showAsAdd ? "newtab-topsites-add-button" : "newtab-topsites-save-button"
    })));
  }

}
TopSiteForm.defaultProps = {
  site: null,
  index: -1
};
;// CONCATENATED MODULE: ./content-src/components/TopSites/TopSites.jsx
function TopSites_extends() { TopSites_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return TopSites_extends.apply(this, arguments); }

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

  if (link.faviconSize >= MIN_RICH_FAVICON_SIZE) {
    return "rich_icon";
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
    screenshot: 0,
    tippytop: 0,
    rich_icon: 0,
    no_image: 0
  });
}

class _TopSites extends (external_React_default()).PureComponent {
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

    this.props.dispatch(actionCreators.AlsoToMain({
      type: actionTypes.SAVE_SESSION_PERF_DATA,
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
    let sitesPerRow = TOP_SITES_MAX_SITES_PER_ROW; // $break-point-widest = 1072px (from _variables.scss)

    if (!__webpack_require__.g.matchMedia(`(min-width: 1072px)`).matches) {
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
    this.props.dispatch(actionCreators.UserEvent({
      source: TOP_SITES_SOURCE,
      event: "TOP_SITES_EDIT_CLOSE"
    }));
    this.props.dispatch({
      type: actionTypes.TOP_SITES_CANCEL_EDIT
    });
  }

  onSearchShortcutsFormClose() {
    this.props.dispatch(actionCreators.UserEvent({
      source: TOP_SITES_SOURCE,
      event: "SEARCH_EDIT_CLOSE"
    }));
    this.props.dispatch({
      type: actionTypes.TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL
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
    const colors = props.Prefs.values["newNewtabExperience.colors"];

    if (props.Prefs.values["improvesearch.topSiteSearchShortcuts"]) {
      extraMenuOptions.push("AddSearchShortcut");
    }

    return /*#__PURE__*/external_React_default().createElement(ComponentPerfTimer, {
      id: "topsites",
      initialized: props.TopSites.initialized,
      dispatch: props.dispatch
    }, /*#__PURE__*/external_React_default().createElement(CollapsibleSection, {
      className: "top-sites",
      id: "topsites",
      title: props.title || {
        id: "newtab-section-header-topsites"
      },
      hideTitle: true,
      extraMenuOptions: extraMenuOptions,
      showPrefName: "feeds.topsites",
      eventSource: TOP_SITES_SOURCE,
      collapsed: false,
      isFixed: props.isFixed,
      isFirst: props.isFirst,
      isLast: props.isLast,
      dispatch: props.dispatch
    }, /*#__PURE__*/external_React_default().createElement(TopSiteList, {
      TopSites: props.TopSites,
      TopSitesRows: props.TopSitesRows,
      dispatch: props.dispatch,
      topSiteIconType: topSiteIconType,
      colors: colors
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "edit-topsites-wrapper"
    }, editForm && /*#__PURE__*/external_React_default().createElement("div", {
      className: "edit-topsites"
    }, /*#__PURE__*/external_React_default().createElement(ModalOverlayWrapper, {
      unstyled: true,
      onClose: this.onEditFormClose,
      innerClassName: "modal"
    }, /*#__PURE__*/external_React_default().createElement(TopSiteForm, TopSites_extends({
      site: props.TopSites.rows[editForm.index],
      onClose: this.onEditFormClose,
      dispatch: this.props.dispatch
    }, editForm)))), showSearchShortcutsForm && /*#__PURE__*/external_React_default().createElement("div", {
      className: "edit-search-shortcuts"
    }, /*#__PURE__*/external_React_default().createElement(ModalOverlayWrapper, {
      unstyled: true,
      onClose: this.onSearchShortcutsFormClose,
      innerClassName: "modal"
    }, /*#__PURE__*/external_React_default().createElement(SearchShortcutsForm, {
      TopSites: props.TopSites,
      onClose: this.onSearchShortcutsFormClose,
      dispatch: this.props.dispatch
    }))))));
  }

}
const TopSites_TopSites = (0,external_ReactRedux_namespaceObject.connect)((state, props) => ({
  TopSites: state.TopSites,
  Prefs: state.Prefs,
  TopSitesRows: state.Prefs.values.topSitesRows
}))(_TopSites);
;// CONCATENATED MODULE: ./content-src/components/Sections/Sections.jsx
function Sections_extends() { Sections_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return Sections_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */











const Sections_VISIBLE = "visible";
const Sections_VISIBILITY_CHANGE_EVENT = "visibilitychange";
const CARDS_PER_ROW_DEFAULT = 3;
const CARDS_PER_ROW_COMPACT_WIDE = 4;
class Section extends (external_React_default()).PureComponent {
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

    if (props.compactCards && __webpack_require__.g.matchMedia(`(min-width: 1072px)`).matches) {
      // If the section has compact cards and the viewport is wide enough, we show
      // 4 columns instead of 3.
      // $break-point-widest = 1072px (from _variables.scss)
      cardsPerRow = CARDS_PER_ROW_COMPACT_WIDE;
    }

    const maxCards = cardsPerRow * this.numRows;
    const cards = props.rows.slice(0, maxCards);

    if (this.needsImpressionStats(cards)) {
      props.dispatch(actionCreators.ImpressionStats({
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

    if (props.document.visibilityState === Sections_VISIBLE) {
      this._dispatchImpressionStats();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(Sections_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
      } // When the page becomes visible, send the impression stats ping if the section isn't collapsed.


      this._onVisibilityChange = () => {
        if (props.document.visibilityState === Sections_VISIBLE) {
          if (!this.props.pref.collapsed) {
            this._dispatchImpressionStats();
          }

          props.document.removeEventListener(Sections_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };

      props.document.addEventListener(Sections_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
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
      this.props.document.removeEventListener(Sections_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
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
      this.props.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.NEW_TAB_REHYDRATED,
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

        cards.push(!usePlaceholder ? /*#__PURE__*/external_React_default().createElement(Card, {
          key: i,
          index: i,
          className: className,
          dispatch: dispatch,
          link: link,
          contextMenuOptions: contextMenuOptions,
          eventSource: eventSource,
          shouldSendImpressionStats: this.props.shouldSendImpressionStats,
          isWebExtension: this.props.isWebExtension
        }) : /*#__PURE__*/external_React_default().createElement(PlaceholderCard, {
          key: i,
          className: className
        }));
      }
    }

    const sectionClassName = ["section", compactCards ? "compact-cards" : "normal-cards"].join(" "); // <Section> <-- React component
    // <section> <-- HTML5 element

    return /*#__PURE__*/external_React_default().createElement(ComponentPerfTimer, this.props, /*#__PURE__*/external_React_default().createElement(CollapsibleSection, {
      className: sectionClassName,
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
    }, !shouldShowEmptyState && /*#__PURE__*/external_React_default().createElement("ul", {
      className: "section-list",
      style: {
        padding: 0
      }
    }, cards), shouldShowEmptyState && /*#__PURE__*/external_React_default().createElement("div", {
      className: "section-empty-state"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "empty-state"
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: emptyState.message
    }, /*#__PURE__*/external_React_default().createElement("p", {
      className: "empty-state-message"
    })))), id === "topstories" && /*#__PURE__*/external_React_default().createElement("div", {
      className: "top-stories-bottom-container"
    }, shouldShowTopics && /*#__PURE__*/external_React_default().createElement("div", {
      className: "wrapper-topics"
    }, /*#__PURE__*/external_React_default().createElement(Topics, {
      topics: this.props.topics
    })), shouldShowPocketCta && /*#__PURE__*/external_React_default().createElement("div", {
      className: "wrapper-cta"
    }, /*#__PURE__*/external_React_default().createElement(PocketLoggedInCta, null)), /*#__PURE__*/external_React_default().createElement("div", {
      className: "wrapper-more-recommendations"
    }, shouldShowReadMore && /*#__PURE__*/external_React_default().createElement(MoreRecommendations, {
      read_more_endpoint: read_more_endpoint
    })))));
  }

}
Section.defaultProps = {
  document: __webpack_require__.g.document,
  rows: [],
  emptyState: {},
  pref: {},
  title: ""
};
const SectionIntl = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Prefs: state.Prefs,
  Pocket: state.Pocket
}))(Section);
class _Sections extends (external_React_default()).PureComponent {
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
        sections.push( /*#__PURE__*/external_React_default().createElement(TopSites_TopSites, commonProps));
      } else {
        const section = enabledSections.find(s => s.id === sectionId);

        if (section) {
          sections.push( /*#__PURE__*/external_React_default().createElement(SectionIntl, Sections_extends({}, section, commonProps)));
        }
      }
    }

    return sections;
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "sections-list"
    }, this.renderSections());
  }

}
const Sections_Sections = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Sections: state.Sections,
  Prefs: state.Prefs
}))(_Sections);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/Highlights/Highlights.jsx
function Highlights_extends() { Highlights_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return Highlights_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class _Highlights extends (external_React_default()).PureComponent {
  render() {
    const section = this.props.Sections.find(s => s.id === "highlights");

    if (!section || !section.enabled) {
      return null;
    }

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-highlights sections-list"
    }, /*#__PURE__*/external_React_default().createElement(SectionIntl, Highlights_extends({}, section, {
      isFixed: true
    })));
  }

}
const Highlights = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Sections: state.Sections
}))(_Highlights);
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/HorizontalRule/HorizontalRule.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class HorizontalRule extends (external_React_default()).PureComponent {
  render() {
    return /*#__PURE__*/external_React_default().createElement("hr", {
      className: "ds-hr"
    });
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/Navigation/Navigation.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




class Navigation_Topic extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(actionCreators.DiscoveryStreamUserEvent({
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
    return /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      onLinkClick: this.onLinkClick,
      className: this.props.className,
      url: url
    }, name);
  }

}
class Navigation extends (external_React_default()).PureComponent {
  render() {
    let links = this.props.links || [];
    const alignment = this.props.alignment || "centered";
    const header = this.props.header || {};
    const english = this.props.locale.startsWith("en-");
    const privacyNotice = this.props.privacyNoticeURL || {};
    const {
      newFooterSection
    } = this.props;
    const className = `ds-navigation ds-navigation-${alignment} ${newFooterSection ? `ds-navigation-new-topics` : ``}`;
    let {
      title
    } = header;

    if (newFooterSection) {
      title = {
        id: "newtab-pocket-new-topics-title"
      };

      if (this.props.extraLinks) {
        links = [...links.slice(0, links.length - 1), ...this.props.extraLinks, links[links.length - 1]];
      }
    }

    return /*#__PURE__*/external_React_default().createElement("div", {
      className: className
    }, title && english ? /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: title
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "ds-navigation-header"
    })) : null, english ? /*#__PURE__*/external_React_default().createElement("ul", null, links && links.map(t => /*#__PURE__*/external_React_default().createElement("li", {
      key: t.name
    }, /*#__PURE__*/external_React_default().createElement(Navigation_Topic, {
      url: t.url,
      name: t.name,
      dispatch: this.props.dispatch
    })))) : null, !newFooterSection ? /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      className: "ds-navigation-privacy",
      url: privacyNotice.url
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: privacyNotice.title
    })) : null, newFooterSection ? /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-navigation-family"
    }, /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon firefox-logo"
    }), /*#__PURE__*/external_React_default().createElement("span", null, "|"), /*#__PURE__*/external_React_default().createElement("span", {
      className: "icon pocket-logo"
    }), /*#__PURE__*/external_React_default().createElement("span", {
      className: "ds-navigation-family-message",
      "data-l10n-id": "newtab-pocket-pocket-firefox-family"
    })) : null);
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/PrivacyLink/PrivacyLink.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



class PrivacyLink extends (external_React_default()).PureComponent {
  render() {
    const {
      properties
    } = this.props;
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-privacy-link"
    }, /*#__PURE__*/external_React_default().createElement(SafeAnchor, {
      url: properties.url
    }, /*#__PURE__*/external_React_default().createElement(FluentOrText, {
      message: properties.title
    })));
  }

}
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamComponents/SectionTitle/SectionTitle.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class SectionTitle extends (external_React_default()).PureComponent {
  render() {
    const {
      header: {
        title,
        subtitle
      }
    } = this.props;
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-section-title"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "title"
    }, title), subtitle ? /*#__PURE__*/external_React_default().createElement("div", {
      className: "subtitle"
    }, subtitle) : null);
  }

}
;// CONCATENATED MODULE: ./content-src/lib/selectLayoutRender.js
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
  const DS_COMPONENTS = ["Message", "TextPromo", "SectionTitle", "Signup", "Navigation", "CardGrid", "CollectionCardGrid", "HorizontalRule", "PrivacyLink"];
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
;// CONCATENATED MODULE: ./content-src/components/DiscoveryStreamBase/DiscoveryStreamBase.jsx
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
class _DiscoveryStreamBase extends (external_React_default()).PureComponent {
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
              console.error(`Bad CSS declaration ${property}: ${value}`);
              rule.style.removeProperty(property);
            }
          }); // Set the actual desired selectors scoped to the component

          const prefix = `.ds-layout > .ds-column:nth-child(${rowIndex + 1}) .ds-column-grid > :nth-child(${componentIndex + 1})`; // NB: Splitting on "," doesn't work with strings with commas, but
          // we're okay with not supporting those selectors

          rule.selectorText = selectors.split(",").map(selector => prefix + ( // Assume :pseudo-classes are for component instead of descendant
          selector[0] === ":" ? "" : " ") + selector).join(","); // CSSOM silently ignores bad selectors, so we'll be noisy instead

          if (rule.selectorText === DUMMY_CSS_SELECTOR) {
            console.error(`Bad CSS selector ${selectors}`);
          }
        });
      });
    });
  }

  renderComponent(component, embedWidth) {
    var _component$header;

    switch (component.type) {
      case "Highlights":
        return /*#__PURE__*/external_React_default().createElement(Highlights, null);

      case "TopSites":
        return /*#__PURE__*/external_React_default().createElement("div", {
          className: "ds-top-sites"
        }, /*#__PURE__*/external_React_default().createElement(TopSites_TopSites, {
          isFixed: true,
          title: (_component$header = component.header) === null || _component$header === void 0 ? void 0 : _component$header.title
        }));

      case "TextPromo":
        return /*#__PURE__*/external_React_default().createElement(DSTextPromo, {
          dispatch: this.props.dispatch,
          type: component.type,
          data: component.data
        });

      case "Signup":
        return /*#__PURE__*/external_React_default().createElement(DSSignup, {
          dispatch: this.props.dispatch,
          type: component.type,
          data: component.data
        });

      case "Message":
        return /*#__PURE__*/external_React_default().createElement(DSMessage, {
          title: component.header && component.header.title,
          subtitle: component.header && component.header.subtitle,
          link_text: component.header && component.header.link_text,
          link_url: component.header && component.header.link_url,
          icon: component.header && component.header.icon,
          essentialReadsHeader: component.essentialReadsHeader,
          editorsPicksHeader: component.editorsPicksHeader
        });

      case "SectionTitle":
        return /*#__PURE__*/external_React_default().createElement(SectionTitle, {
          header: component.header
        });

      case "Navigation":
        return /*#__PURE__*/external_React_default().createElement(Navigation, {
          dispatch: this.props.dispatch,
          links: component.properties.links,
          extraLinks: component.properties.extraLinks,
          alignment: component.properties.alignment,
          explore_topics: component.properties.explore_topics,
          header: component.header,
          locale: this.props.App.locale,
          newFooterSection: component.newFooterSection,
          privacyNoticeURL: component.properties.privacyNoticeURL
        });

      case "CollectionCardGrid":
        const {
          DiscoveryStream
        } = this.props;
        return /*#__PURE__*/external_React_default().createElement(CollectionCardGrid, {
          data: component.data,
          feed: component.feed,
          spocs: DiscoveryStream.spocs,
          placement: component.placement,
          type: component.type,
          items: component.properties.items,
          dismissible: this.props.DiscoveryStream.isCollectionDismissible,
          dispatch: this.props.dispatch
        });

      case "CardGrid":
        return /*#__PURE__*/external_React_default().createElement(CardGrid, {
          title: component.header && component.header.title,
          data: component.data,
          feed: component.feed,
          widgets: component.widgets,
          type: component.type,
          dispatch: this.props.dispatch,
          items: component.properties.items,
          hybridLayout: component.properties.hybridLayout,
          hideCardBackground: component.properties.hideCardBackground,
          fourCardLayout: component.properties.fourCardLayout,
          compactGrid: component.properties.compactGrid,
          essentialReadsHeader: component.properties.essentialReadsHeader,
          onboardingExperience: component.properties.onboardingExperience,
          editorsPicksHeader: component.properties.editorsPicksHeader,
          recentSavesEnabled: this.props.DiscoveryStream.recentSavesEnabled,
          hideDescriptions: this.props.DiscoveryStream.hideDescriptions
        });

      case "HorizontalRule":
        return /*#__PURE__*/external_React_default().createElement(HorizontalRule, null);

      case "PrivacyLink":
        return /*#__PURE__*/external_React_default().createElement(PrivacyLink, {
          properties: component.properties
        });

      default:
        return /*#__PURE__*/external_React_default().createElement("div", null, component.type);
    }
  }

  renderStyles(styles) {
    // Use json string as both the key and styles to render so React knows when
    // to unmount and mount a new instance for new styles.
    const json = JSON.stringify(styles);
    return /*#__PURE__*/external_React_default().createElement("style", {
      key: json,
      "data-styles": json,
      ref: this.onStyleMount
    });
  }

  render() {
    const {
      locale
    } = this.props; // Select layout render data by adding spocs and position to recommendations

    const {
      layoutRender
    } = selectLayoutRender({
      state: this.props.DiscoveryStream,
      prefs: this.props.Prefs.values,
      locale
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
    };
    const privacyLinkComponent = extractComponent("PrivacyLink");
    let learnMore = {
      link: {
        href: message.header.link_url,
        message: message.header.link_text
      }
    };
    let sectionTitle = message.header.title;
    let subTitle = ""; // If we're in one of these experiments, override the default message.
    // For now this is English only.

    if (message.essentialReadsHeader || message.editorsPicksHeader) {
      learnMore = null;
      subTitle = "Recommended By Pocket";

      if (message.essentialReadsHeader) {
        sectionTitle = "Today’s Essential Reads";
      } else if (message.editorsPicksHeader) {
        sectionTitle = "Editor’s Picks";
      }
    } // Render a DS-style TopSites then the rest if any in a collapsible section


    return /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, this.props.DiscoveryStream.isPrivacyInfoModalVisible && /*#__PURE__*/external_React_default().createElement(DSPrivacyModal, {
      dispatch: this.props.dispatch
    }), topSites && this.renderLayout([{
      width: 12,
      components: [topSites]
    }]), sponsoredCollection && this.renderLayout([{
      width: 12,
      components: [sponsoredCollection]
    }]), !!layoutRender.length && /*#__PURE__*/external_React_default().createElement(CollapsibleSection, {
      className: "ds-layout",
      collapsed: topStories.pref.collapsed,
      dispatch: this.props.dispatch,
      id: topStories.id,
      isFixed: true,
      learnMore: learnMore,
      privacyNoticeURL: topStories.privacyNoticeURL,
      showPrefName: topStories.pref.feed,
      title: sectionTitle,
      subTitle: subTitle,
      eventSource: "CARDGRID"
    }, this.renderLayout(layoutRender)), this.renderLayout([{
      width: 12,
      components: [{
        type: "Highlights"
      }]
    }]), privacyLinkComponent && this.renderLayout([{
      width: 12,
      components: [privacyLinkComponent]
    }]));
  }

  renderLayout(layoutRender) {
    const styles = [];
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "discovery-stream ds-layout"
    }, layoutRender.map((row, rowIndex) => /*#__PURE__*/external_React_default().createElement("div", {
      key: `row-${rowIndex}`,
      className: `ds-column ds-column-${row.width}`
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "ds-column-grid"
    }, row.components.map((component, componentIndex) => {
      if (!component) {
        return null;
      }

      styles[rowIndex] = [...(styles[rowIndex] || []), component.styles];
      return /*#__PURE__*/external_React_default().createElement("div", {
        key: `component-${componentIndex}`
      }, this.renderComponent(component, row.width));
    })))), this.renderStyles(styles));
  }

}
const DiscoveryStreamBase = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  DiscoveryStream: state.DiscoveryStream,
  Prefs: state.Prefs,
  Sections: state.Sections,
  document: __webpack_require__.g.document,
  App: state.App
}))(_DiscoveryStreamBase);
;// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/BackgroundsSection/BackgroundsSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

class BackgroundsSection extends (external_React_default()).PureComponent {
  render() {
    return /*#__PURE__*/external_React_default().createElement("div", null);
  }

}
;// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/ContentSection/ContentSection.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


class ContentSection extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onPreferenceSelect = this.onPreferenceSelect.bind(this); // Refs are necessary for dynamically measuring drawer heights for slide animations

    this.topSitesDrawerRef = /*#__PURE__*/external_React_default().createRef();
    this.pocketDrawerRef = /*#__PURE__*/external_React_default().createRef();
  }

  inputUserEvent(eventSource, status) {
    this.props.dispatch(actionCreators.UserEvent({
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
    const eventSource = e.target.getAttribute("eventSource"); // TOP_SITES, TOP_STORIES, HIGHLIGHTS

    let value;

    if (e.target.nodeName === "SELECT") {
      value = parseInt(e.target.value, 10);
    } else if (e.target.nodeName === "INPUT") {
      value = e.target.checked;

      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    } else if (e.target.nodeName === "MOZ-TOGGLE") {
      value = e.target.pressed;

      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    }

    this.props.setPref(prefName, value);
  }

  componentDidMount() {
    this.setDrawerMargins();
  }

  componentDidUpdate() {
    this.setDrawerMargins();
  }

  setDrawerMargins() {
    this.setDrawerMargin(`TOP_SITES`, this.props.enabledSections.topSitesEnabled);
    this.setDrawerMargin(`TOP_STORIES`, this.props.enabledSections.pocketEnabled);
  }

  setDrawerMargin(drawerID, isOpen) {
    let drawerRef;

    if (drawerID === `TOP_SITES`) {
      drawerRef = this.topSitesDrawerRef.current;
    } else if (drawerID === `TOP_STORIES`) {
      drawerRef = this.pocketDrawerRef.current;
    } else {
      return;
    }

    let drawerHeight;

    if (drawerRef) {
      var _window$getComputedSt;

      drawerHeight = parseFloat((_window$getComputedSt = window.getComputedStyle(drawerRef)) === null || _window$getComputedSt === void 0 ? void 0 : _window$getComputedSt.height);

      if (isOpen) {
        drawerRef.style.marginTop = `0`;
      } else {
        drawerRef.style.marginTop = `-${drawerHeight}px`;
      }
    }
  }

  render() {
    const {
      enabledSections,
      mayHaveSponsoredTopSites,
      pocketRegion,
      mayHaveSponsoredStories,
      mayHaveRecentSaves,
      openPreferences
    } = this.props;
    const {
      topSitesEnabled,
      pocketEnabled,
      highlightsEnabled,
      showSponsoredTopSitesEnabled,
      showSponsoredPocketEnabled,
      showRecentSavesEnabled,
      topSitesRowsCount
    } = enabledSections;
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: "home-section"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      id: "shortcuts-section",
      className: "section"
    }, /*#__PURE__*/external_React_default().createElement("moz-toggle", {
      id: "shortcuts-toggle",
      pressed: topSitesEnabled || null,
      onToggle: this.onPreferenceSelect,
      preference: "feeds.topsites",
      eventSource: "TOP_SITES",
      "data-l10n-id": "newtab-custom-shortcuts-toggle",
      "data-l10n-attrs": "label, description"
    }), /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("div", {
      className: "more-info-top-wrapper"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "more-information",
      ref: this.topSitesDrawerRef
    }, /*#__PURE__*/external_React_default().createElement("select", {
      id: "row-selector",
      className: "selector",
      name: "row-count",
      preference: "topSitesRows",
      value: topSitesRowsCount,
      onChange: this.onPreferenceSelect,
      disabled: !topSitesEnabled,
      "aria-labelledby": "custom-shortcuts-title"
    }, /*#__PURE__*/external_React_default().createElement("option", {
      value: "1",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 1}"
    }), /*#__PURE__*/external_React_default().createElement("option", {
      value: "2",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 2}"
    }), /*#__PURE__*/external_React_default().createElement("option", {
      value: "3",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 3}"
    }), /*#__PURE__*/external_React_default().createElement("option", {
      value: "4",
      "data-l10n-id": "newtab-custom-row-selector",
      "data-l10n-args": "{\"num\": 4}"
    })), mayHaveSponsoredTopSites && /*#__PURE__*/external_React_default().createElement("div", {
      className: "check-wrapper",
      role: "presentation"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      id: "sponsored-shortcuts",
      className: "sponsored-checkbox",
      disabled: !topSitesEnabled,
      checked: showSponsoredTopSitesEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "showSponsoredTopSites",
      eventSource: "SPONSORED_TOP_SITES"
    }), /*#__PURE__*/external_React_default().createElement("label", {
      className: "sponsored",
      htmlFor: "sponsored-shortcuts",
      "data-l10n-id": "newtab-custom-sponsored-sites"
    })))))), pocketRegion && /*#__PURE__*/external_React_default().createElement("div", {
      id: "pocket-section",
      className: "section"
    }, /*#__PURE__*/external_React_default().createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default().createElement("moz-toggle", {
      id: "pocket-toggle",
      pressed: pocketEnabled || null,
      onToggle: this.onPreferenceSelect,
      preference: "feeds.section.topstories",
      "aria-describedby": "custom-pocket-subtitle",
      eventSource: "TOP_STORIES",
      "data-l10n-id": "newtab-custom-pocket-toggle",
      "data-l10n-attrs": "label, description"
    })), /*#__PURE__*/external_React_default().createElement("div", null, (mayHaveSponsoredStories || mayHaveRecentSaves) && /*#__PURE__*/external_React_default().createElement("div", {
      className: "more-info-pocket-wrapper"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "more-information",
      ref: this.pocketDrawerRef
    }, mayHaveSponsoredStories && /*#__PURE__*/external_React_default().createElement("div", {
      className: "check-wrapper",
      role: "presentation"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      id: "sponsored-pocket",
      className: "sponsored-checkbox",
      disabled: !pocketEnabled,
      checked: showSponsoredPocketEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "showSponsored",
      eventSource: "POCKET_SPOCS"
    }), /*#__PURE__*/external_React_default().createElement("label", {
      className: "sponsored",
      htmlFor: "sponsored-pocket",
      "data-l10n-id": "newtab-custom-pocket-sponsored"
    })), mayHaveRecentSaves && /*#__PURE__*/external_React_default().createElement("div", {
      className: "check-wrapper",
      role: "presentation"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      id: "recent-saves-pocket",
      className: "sponsored-checkbox",
      disabled: !pocketEnabled,
      checked: showRecentSavesEnabled,
      type: "checkbox",
      onChange: this.onPreferenceSelect,
      preference: "showRecentSaves",
      eventSource: "POCKET_RECENT_SAVES"
    }), /*#__PURE__*/external_React_default().createElement("label", {
      className: "sponsored",
      htmlFor: "recent-saves-pocket",
      "data-l10n-id": "newtab-custom-pocket-show-recent-saves"
    })))))), /*#__PURE__*/external_React_default().createElement("div", {
      id: "recent-section",
      className: "section"
    }, /*#__PURE__*/external_React_default().createElement("label", {
      className: "switch"
    }, /*#__PURE__*/external_React_default().createElement("moz-toggle", {
      id: "highlights-toggle",
      pressed: highlightsEnabled || null,
      onToggle: this.onPreferenceSelect,
      preference: "feeds.section.highlights",
      eventSource: "HIGHLIGHTS",
      "data-l10n-id": "newtab-custom-recent-toggle",
      "data-l10n-attrs": "label, description"
    }))), /*#__PURE__*/external_React_default().createElement("span", {
      className: "divider",
      role: "separator"
    }), /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement("button", {
      id: "settings-link",
      className: "external-link",
      onClick: openPreferences,
      "data-l10n-id": "newtab-custom-settings"
    })));
  }

}
;// CONCATENATED MODULE: ./content-src/components/CustomizeMenu/CustomizeMenu.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */





class _CustomizeMenu extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.onEntered = this.onEntered.bind(this);
    this.onExited = this.onExited.bind(this);
  }

  onEntered() {
    if (this.closeButton) {
      this.closeButton.focus();
    }
  }

  onExited() {
    if (this.openButton) {
      this.openButton.focus();
    }
  }

  render() {
    return /*#__PURE__*/external_React_default().createElement("span", null, /*#__PURE__*/external_React_default().createElement(external_ReactTransitionGroup_namespaceObject.CSSTransition, {
      timeout: 300,
      classNames: "personalize-animate",
      in: !this.props.showing,
      appear: true
    }, /*#__PURE__*/external_React_default().createElement("button", {
      className: "icon icon-settings personalize-button",
      onClick: () => this.props.onOpen(),
      "data-l10n-id": "newtab-personalize-icon-label",
      ref: c => this.openButton = c
    })), /*#__PURE__*/external_React_default().createElement(external_ReactTransitionGroup_namespaceObject.CSSTransition, {
      timeout: 250,
      classNames: "customize-animate",
      in: this.props.showing,
      onEntered: this.onEntered,
      onExited: this.onExited,
      appear: true
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "customize-menu",
      role: "dialog",
      "data-l10n-id": "newtab-personalize-dialog-label"
    }, /*#__PURE__*/external_React_default().createElement("button", {
      onClick: () => this.props.onClose(),
      className: "close-button",
      "data-l10n-id": "newtab-custom-close-button",
      ref: c => this.closeButton = c
    }), /*#__PURE__*/external_React_default().createElement(BackgroundsSection, null), /*#__PURE__*/external_React_default().createElement(ContentSection, {
      openPreferences: this.props.openPreferences,
      setPref: this.props.setPref,
      enabledSections: this.props.enabledSections,
      pocketRegion: this.props.pocketRegion,
      mayHaveSponsoredTopSites: this.props.mayHaveSponsoredTopSites,
      mayHaveSponsoredStories: this.props.mayHaveSponsoredStories,
      mayHaveRecentSaves: this.props.DiscoveryStream.recentSavesEnabled,
      dispatch: this.props.dispatch
    }))));
  }

}
const CustomizeMenu = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  DiscoveryStream: state.DiscoveryStream
}))(_CustomizeMenu);
;// CONCATENATED MODULE: ./content-src/components/Search/Search.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ContentSearchUIController, ContentSearchHandoffUIController */


function Search_extends() { Search_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return Search_extends.apply(this, arguments); }





class _Search extends (external_React_default()).PureComponent {
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
      this.props.dispatch(actionCreators.UserEvent({
        event: "SEARCH"
      }));
    }
  }

  onSearchClick(event) {
    window.gContentSearchController.search(event);
  }

  doSearchHandoff(text) {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.HANDOFF_SEARCH_TO_AWESOMEBAR,
      data: {
        text
      }
    }));
    this.props.dispatch({
      type: actionTypes.FAKE_FOCUS_SEARCH
    });
    this.props.dispatch(actionCreators.UserEvent({
      event: "SEARCH_HANDOFF"
    }));

    if (text) {
      this.props.dispatch({
        type: actionTypes.DISABLE_SEARCH
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
      // BrowserUsageTelemetry.sys.mjs knows to handle events with this name, and
      // can add the appropriate telemetry probes for search. Without the correct
      // name, certain tests like browser_UsageTelemetry_content.js will fail
      // (See github ticket #2348 for more details)
      const healthReportKey = IS_NEWTAB ? "newtab" : "abouthome"; // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)

      const searchSource = IS_NEWTAB ? "newtab" : "homepage"; // gContentSearchController needs to exist as a global so that tests for
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
    return /*#__PURE__*/external_React_default().createElement("div", {
      className: wrapperClassName
    }, this.props.showLogo && /*#__PURE__*/external_React_default().createElement("div", {
      className: "logo-and-wordmark"
    }, /*#__PURE__*/external_React_default().createElement("div", {
      className: "logo"
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "wordmark"
    })), !this.props.handoffEnabled && /*#__PURE__*/external_React_default().createElement("div", {
      className: "search-inner-wrapper"
    }, /*#__PURE__*/external_React_default().createElement("input", {
      id: "newtab-search-text",
      "data-l10n-id": "newtab-search-box-input",
      maxLength: "256",
      ref: this.onInputMount,
      type: "search"
    }), /*#__PURE__*/external_React_default().createElement("button", {
      id: "searchSubmit",
      className: "search-button",
      "data-l10n-id": "newtab-search-box-search-button",
      onClick: this.onSearchClick
    })), this.props.handoffEnabled && /*#__PURE__*/external_React_default().createElement("div", {
      className: "search-inner-wrapper"
    }, /*#__PURE__*/external_React_default().createElement("button", Search_extends({
      className: "search-handoff-button"
    }, this.getHandoffInputL10nAttributes(), {
      ref: this.onSearchHandoffButtonMount,
      onClick: this.onSearchHandoffClick,
      tabIndex: "-1"
    }), /*#__PURE__*/external_React_default().createElement("div", Search_extends({
      className: "fake-textbox"
    }, this.getHandoffTextL10nAttributes())), /*#__PURE__*/external_React_default().createElement("input", {
      type: "search",
      className: "fake-editable",
      tabIndex: "-1",
      "aria-hidden": "true",
      onDrop: this.onSearchHandoffDrop,
      onPaste: this.onSearchHandoffPaste,
      ref: this.onInputMountHandoff
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: "fake-caret"
    }))));
  }

}
const Search_Search = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  Prefs: state.Prefs
}))(_Search);
;// CONCATENATED MODULE: ./content-src/components/Base/Base.jsx
function Base_extends() { Base_extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return Base_extends.apply(this, arguments); }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */











const PrefsButton = ({
  onClick,
  icon
}) => /*#__PURE__*/external_React_default().createElement("div", {
  className: "prefs-button"
}, /*#__PURE__*/external_React_default().createElement("button", {
  className: `icon ${icon || "icon-settings"}`,
  onClick: onClick,
  "data-l10n-id": "newtab-settings-button"
})); // Returns a function will not be continuously triggered when called. The
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

class _Base extends (external_React_default()).PureComponent {
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
    __webpack_require__.g.document.body.className = bodyClassName;
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

    return /*#__PURE__*/external_React_default().createElement(ErrorBoundary, {
      className: "base-content-fallback"
    }, /*#__PURE__*/external_React_default().createElement((external_React_default()).Fragment, null, /*#__PURE__*/external_React_default().createElement(BaseContent, Base_extends({}, this.props, {
      adminContent: this.state
    })), isDevtoolsEnabled ? /*#__PURE__*/external_React_default().createElement(ASRouterAdmin, {
      notifyContent: this.notifyContent
    }) : null));
  }

}
class BaseContent extends (external_React_default()).PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
    this.openCustomizationMenu = this.openCustomizationMenu.bind(this);
    this.closeCustomizationMenu = this.closeCustomizationMenu.bind(this);
    this.handleOnKeyDown = this.handleOnKeyDown.bind(this);
    this.onWindowScroll = debounce(this.onWindowScroll.bind(this), 5);
    this.setPref = this.setPref.bind(this);
    this.state = {
      fixedSearch: false
    };
  }

  componentDidMount() {
    __webpack_require__.g.addEventListener("scroll", this.onWindowScroll);
    __webpack_require__.g.addEventListener("keydown", this.handleOnKeyDown);
  }

  componentWillUnmount() {
    __webpack_require__.g.removeEventListener("scroll", this.onWindowScroll);
    __webpack_require__.g.removeEventListener("keydown", this.handleOnKeyDown);
  }

  onWindowScroll() {
    const prefs = this.props.Prefs.values;
    const SCROLL_THRESHOLD = prefs["logowordmark.alwaysVisible"] ? 179 : 34;

    if (__webpack_require__.g.scrollY > SCROLL_THRESHOLD && !this.state.fixedSearch) {
      this.setState({
        fixedSearch: true
      });
    } else if (__webpack_require__.g.scrollY <= SCROLL_THRESHOLD && this.state.fixedSearch) {
      this.setState({
        fixedSearch: false
      });
    }
  }

  openPreferences() {
    this.props.dispatch(actionCreators.OnlyToMain({
      type: actionTypes.SETTINGS_OPEN
    }));
    this.props.dispatch(actionCreators.UserEvent({
      event: "OPEN_NEWTAB_PREFS"
    }));
  }

  openCustomizationMenu() {
    this.props.dispatch({
      type: actionTypes.SHOW_PERSONALIZE
    });
    this.props.dispatch(actionCreators.UserEvent({
      event: "SHOW_PERSONALIZE"
    }));
  }

  closeCustomizationMenu() {
    if (this.props.App.customizeMenuVisible) {
      this.props.dispatch({
        type: actionTypes.HIDE_PERSONALIZE
      });
      this.props.dispatch(actionCreators.UserEvent({
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
    this.props.dispatch(actionCreators.SetPref(pref, value));
  }

  render() {
    const {
      props
    } = this;
    const {
      App
    } = props;
    const {
      initialized,
      customizeMenuVisible
    } = App;
    const prefs = props.Prefs.values;
    const isDiscoveryStream = props.DiscoveryStream.config && props.DiscoveryStream.config.enabled;
    let filteredSections = props.Sections.filter(section => section.id !== "topstories");
    const pocketEnabled = prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"];
    const noSectionsEnabled = !prefs["feeds.topsites"] && !pocketEnabled && filteredSections.filter(section => section.enabled).length === 0;
    const searchHandoffEnabled = prefs["improvesearch.handoffToAwesomebar"];
    const enabledSections = {
      topSitesEnabled: prefs["feeds.topsites"],
      pocketEnabled: prefs["feeds.section.topstories"],
      highlightsEnabled: prefs["feeds.section.highlights"],
      showSponsoredTopSitesEnabled: prefs.showSponsoredTopSites,
      showSponsoredPocketEnabled: prefs.showSponsored,
      showRecentSavesEnabled: prefs.showRecentSaves,
      topSitesRowsCount: prefs.topSitesRows
    };
    const pocketRegion = prefs["feeds.system.topstories"];
    const mayHaveSponsoredStories = prefs["system.showSponsored"];
    const {
      mayHaveSponsoredTopSites
    } = prefs;
    const outerClassName = ["outer-wrapper", isDiscoveryStream && pocketEnabled && "ds-outer-wrapper-search-alignment", isDiscoveryStream && "ds-outer-wrapper-breakpoint-override", prefs.showSearch && this.state.fixedSearch && !noSectionsEnabled && "fixed-search", prefs.showSearch && noSectionsEnabled && "only-search", prefs["logowordmark.alwaysVisible"] && "visible-logo"].filter(v => v).join(" ");
    const hasSnippet = prefs["feeds.snippets"] && this.props.adminContent && this.props.adminContent.message && this.props.adminContent.message.id;
    return /*#__PURE__*/external_React_default().createElement("div", null, /*#__PURE__*/external_React_default().createElement(CustomizeMenu, {
      onClose: this.closeCustomizationMenu,
      onOpen: this.openCustomizationMenu,
      openPreferences: this.openPreferences,
      setPref: this.setPref,
      enabledSections: enabledSections,
      pocketRegion: pocketRegion,
      mayHaveSponsoredTopSites: mayHaveSponsoredTopSites,
      mayHaveSponsoredStories: mayHaveSponsoredStories,
      showing: customizeMenuVisible
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: outerClassName,
      onClick: this.closeCustomizationMenu
    }, /*#__PURE__*/external_React_default().createElement("main", {
      className: hasSnippet ? "has-snippet" : ""
    }, prefs.showSearch && /*#__PURE__*/external_React_default().createElement("div", {
      className: "non-collapsible-section"
    }, /*#__PURE__*/external_React_default().createElement(ErrorBoundary, null, /*#__PURE__*/external_React_default().createElement(Search_Search, Base_extends({
      showLogo: noSectionsEnabled || prefs["logowordmark.alwaysVisible"],
      handoffEnabled: searchHandoffEnabled
    }, props.Search)))), /*#__PURE__*/external_React_default().createElement(ASRouterUISurface, {
      adminContent: this.props.adminContent,
      appUpdateChannel: this.props.Prefs.values.appUpdateChannel,
      fxaEndpoint: this.props.Prefs.values.fxa_endpoint,
      dispatch: this.props.dispatch
    }), /*#__PURE__*/external_React_default().createElement("div", {
      className: `body-wrapper${initialized ? " on" : ""}`
    }, isDiscoveryStream ? /*#__PURE__*/external_React_default().createElement(ErrorBoundary, {
      className: "borderless-error"
    }, /*#__PURE__*/external_React_default().createElement(DiscoveryStreamBase, {
      locale: props.App.locale
    })) : /*#__PURE__*/external_React_default().createElement(Sections_Sections, null)), /*#__PURE__*/external_React_default().createElement(ConfirmDialog, null))));
  }

}
const Base = (0,external_ReactRedux_namespaceObject.connect)(state => ({
  App: state.App,
  Prefs: state.Prefs,
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Search: state.Search
}))(_Base);
;// CONCATENATED MODULE: ./content-src/lib/detect-user-session-start.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


const detect_user_session_start_VISIBLE = "visible";
const detect_user_session_start_VISIBILITY_CHANGE_EVENT = "visibilitychange";
class DetectUserSessionStart {
  constructor(store, options = {}) {
    this._store = store; // Overrides for testing

    this.document = options.document || __webpack_require__.g.document;
    this._perfService = options.perfService || perfService;
    this._onVisibilityChange = this._onVisibilityChange.bind(this);
  }
  /**
   * sendEventOrAddListener - Notify immediately if the page is already visible,
   *                    or else set up a listener for when visibility changes.
   *                    This is needed for accurate session tracking for telemetry,
   *                    because tabs are pre-loaded.
   */


  sendEventOrAddListener() {
    if (this.document.visibilityState === detect_user_session_start_VISIBLE) {
      // If the document is already visible, to the user, send a notification
      // immediately that a session has started.
      this._sendEvent();
    } else {
      // If the document is not visible, listen for when it does become visible.
      this.document.addEventListener(detect_user_session_start_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
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

      this._store.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.SAVE_SESSION_PERF_DATA,
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
    if (this.document.visibilityState === detect_user_session_start_VISIBLE) {
      this._sendEvent();

      this.document.removeEventListener(detect_user_session_start_VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

}
;// CONCATENATED MODULE: external "Redux"
const external_Redux_namespaceObject = Redux;
;// CONCATENATED MODULE: ./content-src/lib/init-store.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */


const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
const EARLY_QUEUED_ACTIONS = [actionTypes.SAVE_SESSION_PERF_DATA];
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

  if (actionUtils.isSendToMain(action)) {
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
    const isRehydrationRequest = action.type === actionTypes.NEW_TAB_STATE_REQUEST;

    if (isRehydrationRequest) {
      getState.didRequestInitialState = true;
      return next(action);
    }

    if (isMergeStoreAction) {
      getState.didRehydrate = true;
      return next(action);
    } // If init happened after our request was made, we need to re-request


    if (getState.didRequestInitialState && action.type === actionTypes.INIT) {
      return next(actionCreators.AlsoToMain({
        type: actionTypes.NEW_TAB_STATE_REQUEST
      }));
    }

    if (actionUtils.isBroadcastToContent(action) || actionUtils.isSendToOneContent(action) || actionUtils.isSendToPreloaded(action)) {
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
    } else if (actionUtils.isFromMain(action)) {
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
  const store = (0,external_Redux_namespaceObject.createStore)(mergeStateReducer((0,external_Redux_namespaceObject.combineReducers)(reducers)), initialState, __webpack_require__.g.RPMAddMessageListener && (0,external_Redux_namespaceObject.applyMiddleware)(queueEarlyMessageMiddleware, rehydrationMiddleware, messageMiddleware));

  if (__webpack_require__.g.RPMAddMessageListener) {
    __webpack_require__.g.RPMAddMessageListener(INCOMING_MESSAGE_NAME, msg => {
      try {
        store.dispatch(msg.data);
      } catch (ex) {
        console.error("Content msg:", msg, "Dispatch error: ", ex);
        dump(`Content msg: ${JSON.stringify(msg)}\nDispatch error: ${ex}\n${ex.stack}`);
      }
    });
  }

  return store;
}
;// CONCATENATED MODULE: ./content-src/activity-stream.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */








const NewTab = ({
  store
}) => /*#__PURE__*/external_React_default().createElement(external_ReactRedux_namespaceObject.Provider, {
  store: store
}, /*#__PURE__*/external_React_default().createElement(Base, null));
function renderWithoutState() {
  const store = initStore(reducers);
  new DetectUserSessionStart(store).sendEventOrAddListener(); // If this document has already gone into the background by the time we've reached
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
      store.dispatch(actionCreators.AlsoToMain({
        type: actionTypes.NEW_TAB_STATE_REQUEST
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

  external_ReactDOM_default().hydrate( /*#__PURE__*/external_React_default().createElement(NewTab, {
    store: store
  }), document.getElementById("root"));
}
function renderCache(initialState) {
  const store = initStore(reducers, initialState);
  new DetectUserSessionStart(store).sendEventOrAddListener();
  external_ReactDOM_default().hydrate( /*#__PURE__*/external_React_default().createElement(NewTab, {
    store: store
  }), document.getElementById("root"));
}
NewtabRenderUtils = __webpack_exports__;
/******/ })()
;