"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSelectedSource = exports.getSelectedLocation = exports.getSourcesForTabs = exports.getSourceTabs = exports.getTabs = undefined;
exports.initialSourcesState = initialSourcesState;
exports.createSource = createSource;
exports.removeSourceFromTabList = removeSourceFromTabList;
exports.removeSourcesFromTabList = removeSourcesFromTabList;
exports.getBlackBoxList = getBlackBoxList;
exports.getNewSelectedSourceId = getNewSelectedSourceId;
exports.getSource = getSource;
exports.getSourceFromId = getSourceFromId;
exports.getSourceByURL = getSourceByURL;
exports.getGeneratedSource = getGeneratedSource;
exports.getPendingSelectedLocation = getPendingSelectedLocation;
exports.getPrettySource = getPrettySource;
exports.hasPrettySource = hasPrettySource;
exports.getSourceInSources = getSourceInSources;
exports.getSources = getSources;
exports.getSourceList = getSourceList;
exports.getSourceCount = getSourceCount;

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _lodashMove = require("devtools/client/debugger/new/dist/vendors").vendored["lodash-move"];

var _lodashMove2 = _interopRequireDefault(_lodashMove);

var _source = require("../utils/source");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources reducer
 * @module reducers/sources
 */
function initialSourcesState() {
  return {
    sources: {},
    selectedLocation: undefined,
    pendingSelectedLocation: _prefs.prefs.pendingSelectedLocation,
    tabs: restoreTabs()
  };
}

function createSource(source) {
  return {
    id: undefined,
    url: undefined,
    sourceMapURL: undefined,
    isBlackBoxed: false,
    isPrettyPrinted: false,
    isWasm: false,
    text: undefined,
    contentType: "",
    error: undefined,
    loadedState: "unloaded",
    ...source
  };
}

function update(state = initialSourcesState(), action) {
  let location = null;

  switch (action.type) {
    case "UPDATE_SOURCE":
      {
        const source = action.source;
        return updateSource(state, source);
      }

    case "ADD_SOURCE":
      {
        const source = action.source;
        return updateSource(state, source);
      }

    case "ADD_SOURCES":
      {
        return action.sources.reduce((newState, source) => updateSource(newState, source), state);
      }

    case "SET_SELECTED_LOCATION":
      location = { ...action.location,
        url: action.source.url
      };
      _prefs.prefs.pendingSelectedLocation = location;
      return { ...state,
        selectedLocation: {
          sourceId: action.source.id,
          ...action.location
        },
        pendingSelectedLocation: location
      };

    case "CLEAR_SELECTED_LOCATION":
      location = {
        url: ""
      };
      _prefs.prefs.pendingSelectedLocation = location;
      return { ...state,
        selectedLocation: null,
        pendingSelectedLocation: location
      };

    case "SET_PENDING_SELECTED_LOCATION":
      location = {
        url: action.url,
        line: action.line
      };
      _prefs.prefs.pendingSelectedLocation = location;
      return { ...state,
        pendingSelectedLocation: location
      };

    case "ADD_TAB":
      return { ...state,
        tabs: updateTabList(state.tabs, action.url)
      };

    case "MOVE_TAB":
      return { ...state,
        tabs: updateTabList(state.tabs, action.url, action.tabIndex)
      };

    case "CLOSE_TAB":
      _prefs.prefs.tabs = action.tabs;
      return { ...state,
        tabs: action.tabs
      };

    case "CLOSE_TABS":
      _prefs.prefs.tabs = action.tabs;
      return { ...state,
        tabs: action.tabs
      };

    case "LOAD_SOURCE_TEXT":
      return setSourceTextProps(state, action);

    case "BLACKBOX":
      if (action.status === "done") {
        const {
          id,
          url
        } = action.source;
        const {
          isBlackBoxed
        } = action.value;
        updateBlackBoxList(url, isBlackBoxed);
        return updateSource(state, {
          id,
          isBlackBoxed
        });
      }

      break;

    case "NAVIGATE":
      const source = state.selectedLocation && state.sources[state.selectedLocation.sourceId];
      const url = source && source.url;

      if (!url) {
        return initialSourcesState();
      }

      return { ...initialSourcesState(),
        url
      };
  }

  return state;
}

function getTextPropsFromAction(action) {
  const {
    sourceId
  } = action;

  if (action.status === "start") {
    return {
      id: sourceId,
      loadedState: "loading"
    };
  } else if (action.status === "error") {
    return {
      id: sourceId,
      error: action.error,
      loadedState: "loaded"
    };
  }

  return {
    text: action.value.text,
    id: sourceId,
    contentType: action.value.contentType,
    loadedState: "loaded"
  };
} // TODO: Action is coerced to `any` unfortunately because how we type
// asynchronous actions is wrong. The `value` may be null for the
// "start" and "error" states but we don't type it like that. We need
// to rethink how we type async actions.


function setSourceTextProps(state, action) {
  const text = getTextPropsFromAction(action);
  return updateSource(state, text);
}

function updateSource(state, source) {
  if (!source.id) {
    return state;
  }

  const existingSource = state.sources[source.id];
  const updatedSource = existingSource ? { ...existingSource,
    ...source
  } : createSource(source);
  return { ...state,
    sources: { ...state.sources,
      [source.id]: updatedSource
    }
  };
}

function removeSourceFromTabList(tabs, url) {
  return tabs.filter(tab => tab !== url);
}

function removeSourcesFromTabList(tabs, urls) {
  return urls.reduce((t, url) => removeSourceFromTabList(t, url), tabs);
}

function restoreTabs() {
  const prefsTabs = _prefs.prefs.tabs || [];
  return prefsTabs;
}
/**
 * Adds the new source to the tab list if it is not already there
 * @memberof reducers/sources
 * @static
 */


function updateTabList(tabs, url, newIndex) {
  const currentIndex = tabs.indexOf(url);

  if (currentIndex === -1) {
    tabs = [url, ...tabs];
  } else if (newIndex !== undefined) {
    tabs = (0, _lodashMove2.default)(tabs, currentIndex, newIndex);
  }

  _prefs.prefs.tabs = tabs;
  return tabs;
}

function updateBlackBoxList(url, isBlackBoxed) {
  const tabs = getBlackBoxList();
  const i = tabs.indexOf(url);

  if (i >= 0) {
    if (!isBlackBoxed) {
      tabs.splice(i, 1);
    }
  } else if (isBlackBoxed) {
    tabs.push(url);
  }

  _prefs.prefs.tabsBlackBoxed = tabs;
}

function getBlackBoxList() {
  return _prefs.prefs.tabsBlackBoxed || [];
}
/**
 * Gets the next tab to select when a tab closes. Heuristics:
 * 1. if the selected tab is available, it remains selected
 * 2. if it is gone, the next available tab to the left should be active
 * 3. if the first tab is active and closed, select the second tab
 *
 * @memberof reducers/sources
 * @static
 */


function getNewSelectedSourceId(state, availableTabs) {
  const selectedLocation = state.sources.selectedLocation;

  if (!selectedLocation) {
    return "";
  }

  const selectedTab = getSource(state, selectedLocation.sourceId);

  if (!selectedTab) {
    return "";
  }

  if (availableTabs.includes(selectedTab.url)) {
    const sources = state.sources.sources;

    if (!sources) {
      return "";
    }

    const selectedSource = getSourceByURL(state, selectedTab.url);

    if (selectedSource) {
      return selectedSource.id;
    }

    return "";
  }

  const tabUrls = state.sources.tabs;
  const leftNeighborIndex = Math.max(tabUrls.indexOf(selectedTab.url) - 1, 0);
  const lastAvailbleTabIndex = availableTabs.length - 1;
  const newSelectedTabIndex = Math.min(leftNeighborIndex, lastAvailbleTabIndex);
  const availableTab = availableTabs[newSelectedTabIndex];
  const tabSource = getSourceByUrlInSources(state.sources.sources, availableTab);

  if (tabSource) {
    return tabSource.id;
  }

  return "";
} // Selectors
// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.


const getSourcesState = state => state.sources;

function getSource(state, id) {
  return getSourceInSources(getSources(state), id);
}

function getSourceFromId(state, id) {
  return getSourcesState(state).sources[id];
}

function getSourceByURL(state, url) {
  return getSourceByUrlInSources(state.sources.sources, url);
}

function getGeneratedSource(state, source) {
  if (!(0, _devtoolsSourceMap.isOriginalId)(source.id)) {
    return source;
  }

  return getSourceFromId(state, (0, _devtoolsSourceMap.originalToGeneratedId)(source.id));
}

function getPendingSelectedLocation(state) {
  return state.sources.pendingSelectedLocation;
}

function getPrettySource(state, id) {
  const source = getSource(state, id);

  if (!source) {
    return;
  }

  return getSourceByURL(state, (0, _source.getPrettySourceURL)(source.url));
}

function hasPrettySource(state, id) {
  return !!getPrettySource(state, id);
}

function getSourceByUrlInSources(sources, url) {
  if (!url) {
    return null;
  }

  return (0, _lodash.find)(sources, source => source.url === url);
}

function getSourceInSources(sources, id) {
  return sources[id];
}

function getSources(state) {
  return state.sources.sources;
}

function getSourceList(state) {
  return Object.values(getSources(state));
}

function getSourceCount(state) {
  return Object.keys(getSources(state)).length;
}

const getTabs = exports.getTabs = (0, _reselect.createSelector)(getSourcesState, sources => sources.tabs);
const getSourceTabs = exports.getSourceTabs = (0, _reselect.createSelector)(getTabs, getSources, (tabs, sources) => tabs.filter(tab => getSourceByUrlInSources(sources, tab)));
const getSourcesForTabs = exports.getSourcesForTabs = (0, _reselect.createSelector)(getSourceTabs, getSources, (tabs, sources) => {
  return tabs.map(tab => getSourceByUrlInSources(sources, tab)).filter(source => source);
});
const getSelectedLocation = exports.getSelectedLocation = (0, _reselect.createSelector)(getSourcesState, sources => sources.selectedLocation);
const getSelectedSource = exports.getSelectedSource = (0, _reselect.createSelector)(getSelectedLocation, getSources, (selectedLocation, sources) => {
  if (!selectedLocation) {
    return;
  }

  return sources[selectedLocation.sourceId];
});
exports.default = update;