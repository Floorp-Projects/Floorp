"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initialASTState = initialASTState;
exports.getSymbols = getSymbols;
exports.hasSymbols = hasSymbols;
exports.isSymbolsLoading = isSymbolsLoading;
exports.isEmptyLineInSource = isEmptyLineInSource;
exports.getEmptyLines = getEmptyLines;
exports.getPausePoints = getPausePoints;
exports.getPausePoint = getPausePoint;
exports.hasPausePoints = hasPausePoints;
exports.getOutOfScopeLocations = getOutOfScopeLocations;
exports.getPreview = getPreview;
exports.getSourceMetaData = getSourceMetaData;
exports.hasSourceMetaData = hasSourceMetaData;
exports.getInScopeLines = getInScopeLines;
exports.isLineInScope = isLineInScope;

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _ast = require("../utils/ast");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function initialASTState() {
  return (0, _makeRecord2.default)({
    symbols: I.Map(),
    emptyLines: I.Map(),
    outOfScopeLocations: null,
    inScopeLines: null,
    preview: null,
    pausePoints: I.Map(),
    sourceMetaData: I.Map()
  })();
}

function update(state = initialASTState(), action) {
  switch (action.type) {
    case "SET_SYMBOLS":
      {
        const {
          sourceId
        } = action;

        if (action.status === "start") {
          return state.setIn(["symbols", sourceId], {
            loading: true
          });
        }

        const value = action.value;
        return state.setIn(["symbols", sourceId], value);
      }

    case "SET_PAUSE_POINTS":
      {
        const {
          sourceText,
          sourceId,
          pausePoints
        } = action;
        const emptyLines = (0, _ast.findEmptyLines)(sourceText, pausePoints);
        return state.setIn(["pausePoints", sourceId], pausePoints).setIn(["emptyLines", sourceId], emptyLines);
      }

    case "OUT_OF_SCOPE_LOCATIONS":
      {
        return state.set("outOfScopeLocations", action.locations);
      }

    case "IN_SCOPE_LINES":
      {
        return state.set("inScopeLines", action.lines);
      }

    case "CLEAR_SELECTION":
      {
        return state.set("preview", null);
      }

    case "SET_PREVIEW":
      {
        if (action.status == "start") {
          return state.set("preview", {
            updating: true
          });
        }

        if (!action.value) {
          return state.set("preview", null);
        }

        return state.set("preview", _objectSpread({}, action.value, {
          updating: false
        }));
      }

    case "RESUME":
      {
        return state.set("outOfScopeLocations", null);
      }

    case "NAVIGATE":
      {
        return initialASTState();
      }

    case "SET_SOURCE_METADATA":
      {
        return state.setIn(["sourceMetaData", action.sourceId], action.sourceMetaData);
      }

    default:
      {
        return state;
      }
  }
} // NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185


function getSymbols(state, source) {
  if (!source) {
    return null;
  }

  return state.ast.symbols.get(source.id) || null;
}

function hasSymbols(state, source) {
  const symbols = getSymbols(state, source);

  if (!symbols) {
    return false;
  }

  return !symbols.hasOwnProperty("loading");
}

function isSymbolsLoading(state, source) {
  const symbols = getSymbols(state, source);

  if (!symbols) {
    return false;
  }

  return symbols.hasOwnProperty("loading");
}

function isEmptyLineInSource(state, line, selectedSource) {
  const emptyLines = getEmptyLines(state, selectedSource);
  return emptyLines && emptyLines.includes(line);
}

function getEmptyLines(state, source) {
  if (!source) {
    return null;
  }

  return state.ast.emptyLines.get(source.id);
}

function getPausePoints(state, sourceId) {
  return state.ast.pausePoints.get(sourceId);
}

function getPausePoint(state, location) {
  if (!location) {
    return;
  }

  const {
    column,
    line,
    sourceId
  } = location;
  const pausePoints = getPausePoints(state, sourceId);

  if (!pausePoints) {
    return;
  }

  const linePoints = pausePoints[line];
  return linePoints && linePoints[column];
}

function hasPausePoints(state, sourceId) {
  const pausePoints = getPausePoints(state, sourceId);
  return !!pausePoints;
}

function getOutOfScopeLocations(state) {
  return state.ast.get("outOfScopeLocations");
}

function getPreview(state) {
  return state.ast.get("preview");
}

const emptySourceMetaData = {};

function getSourceMetaData(state, sourceId) {
  return state.ast.sourceMetaData.get(sourceId) || emptySourceMetaData;
}

function hasSourceMetaData(state, sourceId) {
  return state.ast.hasIn(["sourceMetaData", sourceId]);
}

function getInScopeLines(state) {
  return state.ast.get("inScopeLines");
}

function isLineInScope(state, line) {
  const linesInScope = state.ast.get("inScopeLines");
  return linesInScope && linesInScope.includes(line);
}

exports.default = update;