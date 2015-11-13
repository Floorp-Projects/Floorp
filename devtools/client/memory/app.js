/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { breakdowns, diffingState } = require("./constants");
const { toggleRecordingAllocationStacks } = require("./actions/allocations");
const { setBreakdownAndRefresh } = require("./actions/breakdown");
const { selectSnapshotForDiffingAndRefresh, toggleDiffing } = require("./actions/diffing");
const { toggleInvertedAndRefresh } = require("./actions/inverted");
const { setFilterStringAndRefresh } = require("./actions/filter");
const { pickFileAndExportSnapshot, pickFileAndImportSnapshotAndCensus } = require("./actions/io");
const { selectSnapshotAndRefresh, takeSnapshotAndCensus } = require("./actions/snapshot");
const { breakdownNameToSpec, getBreakdownDisplayData } = require("./utils");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));
const HeapView = createFactory(require("./components/heap"));
const { app: appModel } = require("./models");

const App = createClass({
  displayName: "memory-tool",

  propTypes: appModel,

  getDefaultProps() {
    return {};
  },

  childContextTypes: {
    front: PropTypes.any,
    heapWorker: PropTypes.any,
    toolbox: PropTypes.any,
  },

  getChildContext() {
    return {
      front: this.props.front,
      heapWorker: this.props.heapWorker,
      toolbox: this.props.toolbox,
    }
  },

  render() {
    let {
      dispatch,
      snapshots,
      front,
      heapWorker,
      breakdown,
      allocations,
      inverted,
      toolbox,
      filter,
      diffing
    } = this.props;

    const selectedSnapshot = snapshots.find(s => s.selected);

    const onClickSnapshotListItem = diffing && diffing.state === diffingState.SELECTING
      ? snapshot => dispatch(selectSnapshotForDiffingAndRefresh(heapWorker, snapshot))
      : snapshot => dispatch(selectSnapshotAndRefresh(heapWorker, snapshot.id));

    return (
      dom.div({ id: "memory-tool" },

        Toolbar({
          snapshots,
          breakdowns: getBreakdownDisplayData(),
          onImportClick: () => dispatch(pickFileAndImportSnapshotAndCensus(heapWorker)),
          onTakeSnapshotClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker)),
          onBreakdownChange: breakdown =>
            dispatch(setBreakdownAndRefresh(heapWorker, breakdownNameToSpec(breakdown))),
          onToggleRecordAllocationStacks: () =>
            dispatch(toggleRecordingAllocationStacks(front)),
          allocations,
          inverted,
          onToggleInverted: () =>
            dispatch(toggleInvertedAndRefresh(heapWorker)),
          filter,
          setFilterString: filterString =>
            dispatch(setFilterStringAndRefresh(filterString, heapWorker)),
          diffing,
          onToggleDiffing: () => dispatch(toggleDiffing())
        }),

        dom.div({ id: "memory-tool-container" },
          List({
            itemComponent: SnapshotListItem,
            items: snapshots,
            onSave: snapshot => dispatch(pickFileAndExportSnapshot(snapshot)),
            onClick: onClickSnapshotListItem,
            diffing,
          }),

          HeapView({
            snapshot: selectedSnapshot,
            diffing,
            onSnapshotClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker)),
            toolbox
          })
        )
      )
    );
  },
});

/**
 * Passed into react-redux's `connect` method that is called on store change
 * and passed to components.
 */
function mapStateToProps (state) {
  return state;
}

module.exports = connect(mapStateToProps)(App);
