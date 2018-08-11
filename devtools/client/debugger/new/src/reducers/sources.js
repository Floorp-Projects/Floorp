"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSelectedSource = exports.getSelectedLocation = exports.getSourceCount = undefined;
exports.initialSourcesState = initialSourcesState;
exports.createSource = createSource;
exports.getBlackBoxList = getBlackBoxList;
exports.getSource = getSource;
exports.getSourceFromId = getSourceFromId;
exports.getSourceByURL = getSourceByURL;
exports.getGeneratedSource = getGeneratedSource;
exports.getPendingSelectedLocation = getPendingSelectedLocation;
exports.getPrettySource = getPrettySource;
exports.hasPrettySource = hasPrettySource;
exports.getSourceByUrlInSources = getSourceByUrlInSources;
exports.getSourceInSources = getSourceInSources;
exports.getSources = getSources;
exports.getUrls = getUrls;
exports.getSourceList = getSourceList;

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _source = require("../utils/source");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _prefs = require("../utils/prefs");

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
    urls: {},
    selectedLocation: undefined,
    pendingSelectedLocation: _prefs.prefs.pendingSelectedLocation
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
  const existingUrls = state.urls[source.url];
  const urls = existingUrls ? [...existingUrls, source.id] : [source.id];
  return { ...state,
    sources: { ...state.sources,
      [source.id]: updatedSource
    },
    urls: { ...state.urls,
      [source.url]: urls
    }
  };
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
  return getSourceByUrlInSources(getSources(state), getUrls(state), url);
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

function getSourceByUrlInSources(sources, urls, url) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);

  if (!foundSources) {
    return null;
  }

  return foundSources[0];
}

function getSourcesByUrlInSources(sources, urls, url) {
  if (!url || !urls[url]) {
    return [];
  }

  return urls[url].map(id => sources[id]);
}

function getSourceInSources(sources, id) {
  return sources[id];
}

function getSources(state) {
  return state.sources.sources;
}

function getUrls(state) {
  return state.sources.urls;
}

function getSourceList(state) {
  return Object.values(getSources(state));
}

const getSourceCount = exports.getSourceCount = (0, _reselect.createSelector)(getSources, sources => Object.keys(sources).length);
const getSelectedLocation = exports.getSelectedLocation = (0, _reselect.createSelector)(getSourcesState, sources => sources.selectedLocation);
const getSelectedSource = exports.getSelectedSource = (0, _reselect.createSelector)(getSelectedLocation, getSources, (selectedLocation, sources) => {
  if (!selectedLocation) {
    return;
  }

  return sources[selectedLocation.sourceId];
});
exports.default = update;