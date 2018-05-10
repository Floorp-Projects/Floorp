"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getHistory = exports.waitForState = exports.makeSymbolDeclaration = exports.makeSourceRecord = exports.makeOriginalSource = exports.makeSource = exports.makeFrame = exports.commonLog = exports.createStore = exports.reducers = exports.selectors = exports.actions = undefined;

var _redux = require("devtools/client/shared/vendor/redux");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _devtoolsSourceMap2 = _interopRequireDefault(_devtoolsSourceMap);

var _reducers = require("../reducers/index");

var _reducers2 = _interopRequireDefault(_reducers);

var _actions = require("../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../selectors/index");

var selectors = _interopRequireWildcard(_selectors);

var _history = require("../test/utils/history");

var _createStore = require("../actions/utils/create-store");

var _createStore2 = _interopRequireDefault(_createStore);

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/**
 * @memberof utils/test-head
 * @static
 */
function createStore(client, initialState = {}, sourceMapsMock) {
  return (0, _createStore2.default)({
    log: false,
    history: (0, _history.getHistory)(),
    makeThunkArgs: args => {
      return _objectSpread({}, args, {
        client,
        sourceMaps: sourceMapsMock !== undefined ? sourceMapsMock : _devtoolsSourceMap2.default
      });
    }
  })((0, _redux.combineReducers)(_reducers2.default), initialState);
}
/**
 * @memberof utils/test-head
 * @static
 */


function commonLog(msg, data = {}) {
  console.log(`[INFO] ${msg} ${JSON.stringify(data)}`);
}

function makeFrame({
  id,
  sourceId
}, opts = {}) {
  return _objectSpread({
    id,
    scope: [],
    location: {
      sourceId,
      line: 4
    }
  }, opts);
}
/**
 * @memberof utils/test-head
 * @static
 */


function makeSource(name, props = {}) {
  return _objectSpread({
    id: name,
    loadedState: "unloaded",
    url: `http://localhost:8000/examples/${name}`
  }, props);
}

function makeOriginalSource(name, props) {
  const source = makeSource(name, props);
  return _objectSpread({}, source, {
    id: `${name}-original`
  });
}

function makeSourceRecord(name, props = {}) {
  return I.Map(makeSource(name, props));
}

function makeFuncLocation(startLine, endLine) {
  if (!endLine) {
    endLine = startLine + 1;
  }

  return {
    start: {
      line: startLine
    },
    end: {
      line: endLine
    }
  };
}

function makeSymbolDeclaration(name, start, end, klass) {
  return {
    id: `${name}:${start}`,
    name,
    location: makeFuncLocation(start, end),
    klass
  };
}
/**
 * @memberof utils/test-head
 * @static
 */


function waitForState(store, predicate) {
  return new Promise(resolve => {
    let ret = predicate(store.getState());

    if (ret) {
      resolve(ret);
    }

    const unsubscribe = store.subscribe(() => {
      ret = predicate(store.getState());

      if (ret) {
        unsubscribe();
        resolve(ret);
      }
    });
  });
}

exports.actions = _actions2.default;
exports.selectors = selectors;
exports.reducers = _reducers2.default;
exports.createStore = createStore;
exports.commonLog = commonLog;
exports.makeFrame = makeFrame;
exports.makeSource = makeSource;
exports.makeOriginalSource = makeOriginalSource;
exports.makeSourceRecord = makeSourceRecord;
exports.makeSymbolDeclaration = makeSymbolDeclaration;
exports.waitForState = waitForState;
exports.getHistory = _history.getHistory;