const { actions, breakdowns } = require("../constants");
const DEFAULT_BREAKDOWN = breakdowns.coarseType.breakdown;

let handlers = Object.create(null);

handlers[actions.SET_BREAKDOWN] = function (_, action) {
  return Object.assign({}, action.breakdown);
};

module.exports = function (state=DEFAULT_BREAKDOWN, action) {
  let handle = handlers[action.type];
  if (handle) {
    return handle(state, action);
  }
  return state;
};
