const { ERROR_TYPE: TASK_ERROR_TYPE } = require("devtools/client/shared/redux/middleware/task");

/**
 * Handle errors dispatched from task middleware and
 * store them so we can check in tests or dump them out.
 */
module.exports = function (state=[], action) {
  switch (action.type) {
    case TASK_ERROR_TYPE:
      return [...state, action.error];
  }
  return state;
};
