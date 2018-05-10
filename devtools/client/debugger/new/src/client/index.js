"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.onConnect = undefined;

var _firefox = require("./firefox");

var firefox = _interopRequireWildcard(_firefox);

var _prefs = require("../utils/prefs");

var _dbg = require("../utils/dbg");

var _bootstrap = require("../utils/bootstrap");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function loadFromPrefs(actions) {
  const {
    pauseOnExceptions,
    pauseOnCaughtExceptions
  } = _prefs.prefs;

  if (pauseOnExceptions || pauseOnCaughtExceptions) {
    return actions.pauseOnExceptions(pauseOnExceptions, pauseOnCaughtExceptions);
  }
}

async function onConnect(connection, {
  services,
  toolboxActions
}) {
  // NOTE: the landing page does not connect to a JS process
  if (!connection) {
    return;
  }

  const commands = firefox.clientCommands;
  const {
    store,
    actions,
    selectors
  } = (0, _bootstrap.bootstrapStore)(commands, {
    services,
    toolboxActions
  });
  const workers = (0, _bootstrap.bootstrapWorkers)();
  await firefox.onConnect(connection, actions);
  await loadFromPrefs(actions);
  (0, _dbg.setupHelper)({
    store,
    actions,
    selectors,
    workers: _objectSpread({}, workers, services),
    connection,
    client: firefox.clientCommands
  });
  (0, _bootstrap.bootstrapApp)(store);
  return {
    store,
    actions,
    selectors,
    client: commands
  };
}

exports.onConnect = onConnect;