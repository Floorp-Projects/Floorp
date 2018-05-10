"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.promise = exports.PROMISE = undefined;

var _lodash = require("devtools/client/shared/vendor/lodash");

var _DevToolsUtils = require("../../../utils/DevToolsUtils");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

let seqIdVal = 1;

function seqIdGen() {
  return seqIdVal++;
}

function filterAction(action) {
  return (0, _lodash.fromPairs)((0, _lodash.toPairs)(action).filter(pair => pair[0] !== PROMISE));
}

function promiseMiddleware({
  dispatch,
  getState
}) {
  return next => action => {
    if (!(PROMISE in action)) {
      return next(action);
    }

    const promiseInst = action[PROMISE];
    const seqId = seqIdGen().toString(); // Create a new action that doesn't have the promise field and has
    // the `seqId` field that represents the sequence id

    action = _objectSpread({}, filterAction(action), {
      seqId
    });
    dispatch(_objectSpread({}, action, {
      status: "start"
    })); // Return the promise so action creators can still compose if they
    // want to.

    return new Promise((resolve, reject) => {
      promiseInst.then(value => {
        (0, _DevToolsUtils.executeSoon)(() => {
          dispatch(_objectSpread({}, action, {
            status: "done",
            value: value
          }));
          resolve(value);
        });
      }, error => {
        (0, _DevToolsUtils.executeSoon)(() => {
          dispatch(_objectSpread({}, action, {
            status: "error",
            error: error.message || error
          }));
          reject(error);
        });
      });
    });
  };
}

const PROMISE = exports.PROMISE = "@@dispatch/promise";
exports.promise = promiseMiddleware;