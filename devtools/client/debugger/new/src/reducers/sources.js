"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSelectedSourceText = exports.getSelectedSource = exports.getSelectedLocation = exports.getSourcesForTabs = exports.getSourceTabs = exports.getTabs = exports.getSources = exports.RelativeSourceRecordClass = exports.SourceRecordClass = undefined;
exports.initialSourcesState = initialSourcesState;
exports.createSourceRecord = createSourceRecord;
exports.removeSourceFromTabList = removeSourceFromTabList;
exports.removeSourcesFromTabList = removeSourcesFromTabList;
exports.getBlackBoxList = getBlackBoxList;
exports.getNewSelectedSourceId = getNewSelectedSourceId;
exports.getSource = getSource;
exports.getSourceByURL = getSourceByURL;
exports.getGeneratedSource = getGeneratedSource;
exports.getPendingSelectedLocation = getPendingSelectedLocation;
exports.getPrettySource = getPrettySource;
exports.hasPrettySource = hasPrettySource;
exports.getSourceInSources = getSourceInSources;

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _source = require("../utils/source");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function initialSourcesState() {
  return (0, _makeRecord2.default)({
    sources: I.Map(),
    selectedLocation: undefined,
    pendingSelectedLocation: _prefs.prefs.pendingSelectedLocation,
    sourcesText: I.Map(),
    tabs: I.List(restoreTabs())
  })();
}

const sourceRecordProperties = {
  id: undefined,
  url: undefined,
  sourceMapURL: undefined,
  isBlackBoxed: false,
  isPrettyPrinted: false,
  isWasm: false,
  text: undefined,
  contentType: "",
  error: undefined,
  loadedState: "unloaded"
};
const SourceRecordClass = exports.SourceRecordClass = new I.Record(sourceRecordProperties);
const RelativeSourceRecordClass = exports.RelativeSourceRecordClass = new I.Record(_objectSpread({}, sourceRecordProperties, {
  relativeUrl: undefined
}));

function createSourceRecord(source) {
  return new SourceRecordClass(source);
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
      location = _objectSpread({}, action.location, {
        url: action.source.url
      });
      _prefs.prefs.pendingSelectedLocation = location;
      return state.set("selectedLocation", _objectSpread({
        sourceId: action.source.id
      }, action.location)).set("pendingSelectedLocation", location);

    case "CLEAR_SELECTED_LOCATION":
      location = {
        url: ""
      };
      _prefs.prefs.pendingSelectedLocation = location;
      return state.set("selectedLocation", {
        sourceId: ""
      }).set("pendingSelectedLocation", location);

    case "SET_PENDING_SELECTED_LOCATION":
      location = {
        url: action.url,
        line: action.line
      };
      _prefs.prefs.pendingSelectedLocation = location;
      return state.set("pendingSelectedLocation", location);

    case "ADD_TAB":
      return state.merge({
        tabs: updateTabList({
          sources: state
        }, action.url)
      });

    case "MOVE_TAB":
      return state.merge({
        tabs: updateTabList({
          sources: state
        }, action.url, action.tabIndex)
      });

    case "CLOSE_TAB":
      _prefs.prefs.tabs = action.tabs;
      return state.merge({
        tabs: action.tabs
      });

    case "CLOSE_TABS":
      _prefs.prefs.tabs = action.tabs;
      return state.merge({
        tabs: action.tabs
      });

    case "LOAD_SOURCE_TEXT":
      return setSourceTextProps(state, action);

    case "BLACKBOX":
      if (action.status === "done") {
        const url = action.source.url;
        const {
          isBlackBoxed
        } = action.value;
        updateBlackBoxList(url, isBlackBoxed);
        return state.setIn(["sources", action.source.id, "isBlackBoxed"], isBlackBoxed);
      }

      break;

    case "NAVIGATE":
      const source = getSelectedSource({
        sources: state
      });
      const url = source && source.url;

      if (!url) {
        return initialSourcesState();
      }

      return initialSourcesState().set("pendingSelectedLocation", {
        url
      });
  }

  return state;
}

function getTextPropsFromAction(action) {
  const {
    value,
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
    text: value.text,
    id: sourceId,
    contentType: value.contentType,
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

  const existingSource = state.sources.get(source.id);

  if (existingSource) {
    const updatedSource = existingSource.merge(source);
    return state.setIn(["sources", source.id], updatedSource);
  }

  return state.setIn(["sources", source.id], createSourceRecord(source));
}

function removeSourceFromTabList(tabs, url) {
  return tabs.filter(tab => tab != url);
}

function removeSourcesFromTabList(tabs, urls) {
  return urls.reduce((t, url) => removeSourceFromTabList(t, url), tabs);
}

function restoreTabs() {
  const prefsTabs = _prefs.prefs.tabs || [];

  if (prefsTabs.length == 0) {
    return;
  }

  return prefsTabs;
}
/**
 * Adds the new source to the tab list if it is not already there
 * @memberof reducers/sources
 * @static
 */


function updateTabList(state, url, tabIndex) {
  let tabs = state.sources.tabs;
  const urlIndex = tabs.indexOf(url);
  const includesUrl = !!tabs.find(tab => tab == url);

  if (includesUrl) {
    if (tabIndex != undefined) {
      tabs = tabs.delete(urlIndex).insert(tabIndex, url);
    }
  } else {
    tabs = tabs.insert(0, url);
  }

  _prefs.prefs.tabs = tabs.toJS();
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

  const selectedTab = state.sources.sources.get(selectedLocation.sourceId);
  const selectedTabUrl = selectedTab ? selectedTab.url : "";

  if (availableTabs.includes(selectedTabUrl)) {
    const sources = state.sources.sources;

    if (!sources) {
      return "";
    }

    const selectedSource = sources.find(source => source.url == selectedTabUrl);

    if (selectedSource) {
      return selectedSource.id;
    }

    return "";
  }

  const tabUrls = state.sources.tabs;
  const leftNeighborIndex = Math.max(tabUrls.indexOf(selectedTabUrl) - 1, 0);
  const lastAvailbleTabIndex = availableTabs.size - 1;
  const newSelectedTabIndex = Math.min(leftNeighborIndex, lastAvailbleTabIndex);
  const availableTab = availableTabs.get(newSelectedTabIndex);
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

function getSourceByURL(state, url) {
  return getSourceByUrlInSources(state.sources.sources, url);
}

function getGeneratedSource(state, sourceRecord) {
  if (!sourceRecord || !(0, _devtoolsSourceMap.isOriginalId)(sourceRecord.id)) {
    return null;
  }

  return getSource(state, (0, _devtoolsSourceMap.originalToGeneratedId)(sourceRecord.id));
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

  return sources.find(source => source.url === url);
}

function getSourceInSources(sources, id) {
  return sources.get(id);
}

const getSources = exports.getSources = (0, _reselect.createSelector)(getSourcesState, sources => sources.sources);
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

  return sources.get(selectedLocation.sourceId);
});
const getSelectedSourceText = exports.getSelectedSourceText = (0, _reselect.createSelector)(getSelectedSource, getSourcesState, (selectedSource, sources) => {
  const id = selectedSource.id;
  return id ? sources.sourcesText.get(id) : null;
});
exports.default = update;