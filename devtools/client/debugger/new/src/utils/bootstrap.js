"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.bootstrapStore = bootstrapStore;
exports.bootstrapWorkers = bootstrapWorkers;
exports.teardownWorkers = teardownWorkers;
exports.bootstrapApp = bootstrapApp;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _redux = require("devtools/client/shared/vendor/redux");

var _reactDom = require("devtools/client/shared/vendor/react-dom");

var _reactDom2 = _interopRequireDefault(_reactDom);

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _search = require("../workers/search/index");

var search = _interopRequireWildcard(_search);

var _prettyPrint = require("../workers/pretty-print/index");

var prettyPrint = _interopRequireWildcard(_prettyPrint);

var _parser = require("../workers/parser/index");

var parser = _interopRequireWildcard(_parser);

var _createStore = require("../actions/utils/create-store");

var _createStore2 = _interopRequireDefault(_createStore);

var _reducers = require("../reducers/index");

var _reducers2 = _interopRequireDefault(_reducers);

var _selectors = require("../selectors/index");

var selectors = _interopRequireWildcard(_selectors);

var _App = require("../components/App");

var _App2 = _interopRequireDefault(_App);

var _prefs = require("./prefs");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

const {
  Provider
} = require("devtools/client/shared/vendor/react-redux");

function renderPanel(component, store) {
  const root = document.createElement("div");
  root.className = "launchpad-root theme-body";
  root.style.setProperty("flex", "1");
  const mount = document.querySelector("#mount");

  if (!mount) {
    return;
  }

  mount.appendChild(root);

  _reactDom2.default.render(_react2.default.createElement(Provider, {
    store
  }, _react2.default.createElement(component)), root);
}

function bootstrapStore(client, {
  services,
  toolboxActions
}) {
  const createStore = (0, _createStore2.default)({
    log: (0, _devtoolsEnvironment.isTesting)(),
    timing: (0, _devtoolsEnvironment.isDevelopment)(),
    makeThunkArgs: (args, state) => {
      return _objectSpread({}, args, {
        client
      }, services, toolboxActions);
    }
  });
  const store = createStore((0, _redux.combineReducers)(_reducers2.default));
  store.subscribe(() => updatePrefs(store.getState()));
  const actions = (0, _redux.bindActionCreators)(require("../actions/index").default, store.dispatch);
  return {
    store,
    actions,
    selectors
  };
}

function bootstrapWorkers() {
  const workerPath = (0, _devtoolsEnvironment.isDevelopment)() ? "assets/build" : "resource://devtools/client/debugger/new/dist";

  if ((0, _devtoolsEnvironment.isDevelopment)()) {
    // When used in Firefox, the toolbox manages the source map worker.
    (0, _devtoolsSourceMap.startSourceMapWorker)(`${workerPath}/source-map-worker.js`);
  }

  prettyPrint.start(`${workerPath}/pretty-print-worker.js`);
  parser.start(`${workerPath}/parser-worker.js`);
  search.start(`${workerPath}/search-worker.js`);
  return {
    prettyPrint,
    parser,
    search
  };
}

function teardownWorkers() {
  if (!(0, _devtoolsEnvironment.isFirefoxPanel)()) {
    // When used in Firefox, the toolbox manages the source map worker.
    (0, _devtoolsSourceMap.stopSourceMapWorker)();
  }

  prettyPrint.stop();
  parser.stop();
  search.stop();
}

function bootstrapApp(store) {
  if ((0, _devtoolsEnvironment.isFirefoxPanel)()) {
    renderPanel(_App2.default, store);
  } else {
    const {
      renderRoot
    } = require("devtools/shared/flags");

    renderRoot(_react2.default, _reactDom2.default, _App2.default, store);
  }
}

function updatePrefs(state) {
  const pendingBreakpoints = selectors.getPendingBreakpoints(state);

  if (_prefs.prefs.pendingBreakpoints !== pendingBreakpoints) {
    _prefs.prefs.pendingBreakpoints = pendingBreakpoints;
  }
}