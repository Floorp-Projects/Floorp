/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { breakdowns } = require("./constants");
const { toggleRecordingAllocationStacks } = require("./actions/allocations");
const { setBreakdownAndRefresh } = require("./actions/breakdown");
const { toggleInvertedAndRefresh } = require("./actions/inverted");
const { setFilterStringAndRefresh } = require("./actions/filter");
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
    } = this.props;

    let selectedSnapshot = snapshots.find(s => s.selected);

    return (
      dom.div({ id: "memory-tool" },

        Toolbar({
          breakdowns: getBreakdownDisplayData(),
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
        }),

        dom.div({ id: "memory-tool-container" },
          List({
            itemComponent: SnapshotListItem,
            items: snapshots,
            onClick: snapshot => dispatch(selectSnapshotAndRefresh(heapWorker, snapshot))
          }),

          HeapView({
            snapshot: selectedSnapshot,
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
