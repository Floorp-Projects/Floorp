const { actions } = require("../constants");
const { PROMISE } = require("devtools/client/shared/redux/middleware/promise");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

function handleTakeSnapshot (state, action) {
  switch (action.status) {

    case "start":
      return [...state, {
        id: action.seqId,
        status: action.status,
        // auto selected if this is the first snapshot
        selected: state.length === 0
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

function handleSelectSnapshot (state, action) {
  let selected = state.find(s => s.id === action.snapshot.id);

  if (!selected) {
    DevToolsUtils.reportException(`Cannot select non-existant snapshot ${snapshot.id}`);
  }

  return state.map(s => {
    s.selected = s === selected;
    return s;
  });
}

module.exports = function (state=[], action) {
  switch (action.type) {
    case actions.TAKE_SNAPSHOT:
      return handleTakeSnapshot(state, action);
    case actions.SELECT_SNAPSHOT:
      return handleSelectSnapshot(state, action);
  }

  return state;
};
