const { actions } = require("../constants");

// Hardcoded breakdown for now
const DEFAULT_BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

/**
 * Not much to do here yet until we can change breakdowns,
 * but this gets it in our store.
 */
module.exports = function (state=DEFAULT_BREAKDOWN, action) {
  return Object.assign({}, DEFAULT_BREAKDOWN);
};
