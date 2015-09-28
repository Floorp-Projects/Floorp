const { actions } = require("../constants");
const { PROMISE } = require("devtools/client/shared/redux/middleware/promise");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

function handleTakeSnapshot (state, action) {
  switch (action.status) {

    case "start":
      return [...state, {
        id: action.seqId,
        status: action.status
      }];

    case "done":
      let snapshot = state.find(s => s.id === action.seqId);
      if (!snapshot) {
        DevToolsUtils.reportException(`No snapshot with id "${action.seqId}" for TAKE_SNAPSHOT`);
        break;
      }
      snapshot.status = "done";
      snapshot.snapshotId = action.value;
      return [...state];

    case "error":
      DevToolsUtils.reportException(`No async state found for ${action.type}`);
  }
  return [...state];
}

module.exports = function (state=[], action) {
  switch (action.type) {
    case actions.TAKE_SNAPSHOT:
      return handleTakeSnapshot(state, action);
  }

  return state;
};
