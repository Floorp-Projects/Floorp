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
exports.getOriginalSourceByURL = getOriginalSourceByURL;
exports.getGeneratedSourceByURL = getGeneratedSourceByURL;
exports.getSpecificSourceByURL = getSpecificSourceByURL;
exports.getSourceByURL = getSourceByURL;
exports.getSourcesByURLs = getSourcesByURLs;
exports.getSourcesByURL = getSourcesByURL;
exports.getGeneratedSource = getGeneratedSource;
exports.getPendingSelectedLocation = getPendingSelectedLocation;
exports.getPrettySource = getPrettySource;
exports.hasPrettySource = hasPrettySource;
exports.getOriginalSourceByUrlInSources = getOriginalSourceByUrlInSources;
exports.getGeneratedSourceByUrlInSources = getGeneratedSourceByUrlInSources;
exports.getSpecificSourceByUrlInSources = getSpecificSourceByUrlInSources;
exports.getSourceByUrlInSources = getSourceByUrlInSources;
exports.getSourcesUrlsInSources = getSourcesUrlsInSources;
exports.getSourceInSources = getSourceInSources;
exports.getSources = getSources;
exports.getUrls = getUrls;
exports.getSourceList = getSourceList;
exports.getProjectDirectoryRoot = getProjectDirectoryRoot;
exports.getRelativeSources = getRelativeSources;

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
    relativeSources: {},
    selectedLocation: undefined,
    pendingSelectedLocation: _prefs.prefs.pendingSelectedLocation,
    projectDirectoryRoot: _prefs.prefs.projectDirectoryRoot
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
        return updateSources(state, [source]);
      }

    case "ADD_SOURCE":
      {
        const source = action.source;
        return updateSources(state, [source]);
      }

    case "ADD_SOURCES":
      {
        return updateSources(state, action.sources);
      }

    case "SET_SELECTED_LOCATION":
      location = { ...action.location,
        url: action.source.url
      };

      if (action.source.url) {
        _prefs.prefs.pendingSelectedLocation = location;
      }

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
        return updateSources(state, [{
          id,
          isBlackBoxed
        }]);
      }

      break;

    case "SET_PROJECT_DIRECTORY_ROOT":
      return recalculateRelativeSources(state, action.url);

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

  if (!action.value) {
    return null;
  }

  return {
    id: sourceId,
    text: action.value.text,
    contentType: action.value.contentType,
    loadedState: "loaded"
  };
} // TODO: Action is coerced to `any` unfortunately because how we type
// asynchronous actions is wrong. The `value` may be null for the
// "start" and "error" states but we don't type it like that. We need
// to rethink how we type async actions.


function setSourceTextProps(state, action) {
  const source = getTextPropsFromAction(action);

  if (!source) {
    return state;
  }

  return updateSources(state, [source]);
}

function updateSources(state, sources) {
  state = { ...state,
    sources: { ...state.sources
    },
    relativeSources: { ...state.relativeSources
    },
    urls: { ...state.urls
    }
  };
  return sources.reduce((newState, source) => updateSource(newState, source), state);
}

function updateSource(state, source) {
  if (!source.id) {
    return state;
  }

  const existingSource = state.sources[source.id];
  const updatedSource = existingSource ? { ...existingSource,
    ...source
  } : createSource(source);
  state.sources[source.id] = updatedSource;
  const existingUrls = state.urls[source.url];
  state.urls[source.url] = existingUrls ? [...existingUrls, source.id] : [source.id];
  updateRelativeSource(state.relativeSources, updatedSource, state.projectDirectoryRoot);
  return state;
}

function updateRelativeSource(relativeSources, source, root) {
  if (!(0, _source.underRoot)(source, root)) {
    return relativeSources;
  }

  const relativeSource = { ...source,
    relativeUrl: (0, _source.getRelativeUrl)(source, root)
  };
  relativeSources[source.id] = relativeSource;
  return relativeSources;
}

function recalculateRelativeSources(state, root) {
  _prefs.prefs.projectDirectoryRoot = root;
  const relativeSources = Object.values(state.sources).reduce((sources, source) => updateRelativeSource(sources, source, root), {});
  return { ...state,
    projectDirectoryRoot: root,
    relativeSources
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

function getOriginalSourceByURL(state, url) {
  return getOriginalSourceByUrlInSources(getSources(state), getUrls(state), url);
}

function getGeneratedSourceByURL(state, url) {
  return getGeneratedSourceByUrlInSources(getSources(state), getUrls(state), url);
}

function getSpecificSourceByURL(state, url, isOriginal) {
  return isOriginal ? getOriginalSourceByUrlInSources(getSources(state), getUrls(state), url) : getGeneratedSourceByUrlInSources(getSources(state), getUrls(state), url);
}

function getSourceByURL(state, url) {
  return getSourceByUrlInSources(getSources(state), getUrls(state), url);
}

function getSourcesByURLs(state, urls) {
  return urls.map(url => getSourceByURL(state, url)).filter(Boolean);
}

function getSourcesByURL(state, url) {
  return getSourcesByUrlInSources(getSources(state), getUrls(state), url);
}

function getGeneratedSource(state, source) {
  if ((0, _source.isGenerated)(source)) {
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

  return getSpecificSourceByURL(state, (0, _source.getPrettySourceURL)(source.url), true);
}

function hasPrettySource(state, id) {
  return !!getPrettySource(state, id);
}

function getOriginalSourceByUrlInSources(sources, urls, url) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);

  if (!foundSources) {
    return null;
  }

  return foundSources.find(source => (0, _source.isOriginal)(source) == true);
}

function getGeneratedSourceByUrlInSources(sources, urls, url) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);

  if (!foundSources) {
    return null;
  }

  return foundSources.find(source => (0, _source.isOriginal)(source) == false);
}

function getSpecificSourceByUrlInSources(sources, urls, url, isOriginal) {
  return isOriginal ? getOriginalSourceByUrlInSources(sources, urls, url) : getGeneratedSourceByUrlInSources(sources, urls, url);
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

function getSourcesUrlsInSources(state, url) {
  const urls = getUrls(state);

  if (!url || !urls[url]) {
    return [];
  }

  return [...new Set(Object.keys(urls).filter(Boolean))];
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

function getProjectDirectoryRoot(state) {
  return state.sources.projectDirectoryRoot;
}

function getRelativeSources(state) {
  return state.sources.relativeSources;
}

exports.default = update;