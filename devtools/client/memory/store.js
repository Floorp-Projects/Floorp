const { combineReducers } = require("../shared/vendor/redux");
const createStore = require("../shared/redux/create-store");
const reducers = require("./reducers");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

module.exports = function () {
  let shouldLog = DevToolsUtils.testing;
  return createStore({ log: shouldLog })(combineReducers(reducers), {});
};
