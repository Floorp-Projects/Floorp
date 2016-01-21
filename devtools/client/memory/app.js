/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { assert } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { breakdowns, diffingState, viewState } = require("./constants");
const { toggleRecordingAllocationStacks } = require("./actions/allocations");
const { setBreakdownAndRefresh } = require("./actions/breakdown");
const { setDominatorTreeBreakdownAndRefresh } = require("./actions/dominatorTreeBreakdown");
const {
  selectSnapshotForDiffingAndRefresh,
  toggleDiffing,
  expandDiffingCensusNode,
  collapseDiffingCensusNode,
  focusDiffingCensusNode,
} = require("./actions/diffing");
const { toggleInvertedAndRefresh } = require("./actions/inverted");
const { setFilterStringAndRefresh } = require("./actions/filter");
const { pickFileAndExportSnapshot, pickFileAndImportSnapshotAndCensus } = require("./actions/io");
const {
  selectSnapshotAndRefresh,
  takeSnapshotAndCensus,
  clearSnapshots,
  fetchImmediatelyDominated,
  expandCensusNode,
  collapseCensusNode,
  focusCensusNode,
  expandDominatorTreeNode,
  collapseDominatorTreeNode,
  focusDominatorTreeNode,
} = require("./actions/snapshot");
const { changeViewAndRefresh } = require("./actions/view");
const {
  breakdownNameToSpec,
  getBreakdownDisplayData,
  dominatorTreeBreakdownNameToSpec,
  getDominatorTreeBreakdownDisplayData,
} = require("./utils");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));
const Heap = createFactory(require("./components/heap"));
const { app: appModel } = require("./models");

const MemoryApp = createClass({
  displayName: "MemoryApp",

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
    };
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
      diffing,
      view,
      dominatorTreeBreakdown
    } = this.props;

    const selectedSnapshot = snapshots.find(s => s.selected);

    const onClickSnapshotListItem = diffing && diffing.state === diffingState.SELECTING
      ? snapshot => dispatch(selectSnapshotForDiffingAndRefresh(heapWorker, snapshot))
      : snapshot => dispatch(selectSnapshotAndRefresh(heapWorker, snapshot.id));

    return (
      dom.div(
        {
          id: "memory-tool"
        },

        Toolbar({
          snapshots,
          breakdowns: getBreakdownDisplayData(),
          onImportClick: () => dispatch(pickFileAndImportSnapshotAndCensus(heapWorker)),
          onClearSnapshotsClick: () => dispatch(clearSnapshots(heapWorker)),
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
          onToggleDiffing: () => dispatch(toggleDiffing()),
          view,
          dominatorTreeBreakdowns: getDominatorTreeBreakdownDisplayData(),
          onDominatorTreeBreakdownChange: breakdown => {
            const spec = dominatorTreeBreakdownNameToSpec(breakdown);
            assert(spec, "Should have a breakdown spec");
            dispatch(setDominatorTreeBreakdownAndRefresh(heapWorker, spec));
          },
          onViewChange: v => dispatch(changeViewAndRefresh(v, heapWorker)),
        }),

        dom.div(
          {
            id: "memory-tool-container"
          },

          List({
            itemComponent: SnapshotListItem,
            items: snapshots,
            onSave: snapshot => dispatch(pickFileAndExportSnapshot(snapshot)),
            onClick: onClickSnapshotListItem,
            diffing,
          }),

          Heap({
            snapshot: selectedSnapshot,
            diffing,
            onViewSourceInDebugger: frame => toolbox.viewSourceInDebugger(frame.source, frame.line),
            onSnapshotClick: () =>
              dispatch(takeSnapshotAndCensus(front, heapWorker)),
            onLoadMoreSiblings: lazyChildren =>
              dispatch(fetchImmediatelyDominated(heapWorker,
                                                 selectedSnapshot.id,
                                                 lazyChildren)),
            onCensusExpand: (census, node) => {
              if (diffing) {
                assert(diffing.census === census,
                       "Should only expand active census");
                dispatch(expandDiffingCensusNode(node));
              } else {
                assert(selectedSnapshot && selectedSnapshot.census === census,
                       "If not diffing, should be expanding on selected snapshot's census");
                dispatch(expandCensusNode(selectedSnapshot.id, node));
              }
            },
            onCensusCollapse: (census, node) => {
              if (diffing) {
                assert(diffing.census === census,
                       "Should only collapse active census");
                dispatch(collapseDiffingCensusNode(node));
              } else {
                assert(selectedSnapshot && selectedSnapshot.census === census,
                       "If not diffing, should be collapsing on selected snapshot's census");
                dispatch(collapseCensusNode(selectedSnapshot.id, node));
              }
            },
            onCensusFocus: (census, node) => {
              if (diffing) {
                assert(diffing.census === census,
                       "Should only focus nodes in active census");
                dispatch(focusDiffingCensusNode(node));
              } else {
                assert(selectedSnapshot && selectedSnapshot.census === census,
                       "If not diffing, should be focusing on nodes in selected snapshot's census");
                dispatch(focusCensusNode(selectedSnapshot.id, node));
              }
            },
            onDominatorTreeExpand: node => {
              assert(view === viewState.DOMINATOR_TREE,
                     "If expanding dominator tree nodes, should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(expandDominatorTreeNode(selectedSnapshot.id, node));
            },
            onDominatorTreeCollapse: node => {
              assert(view === viewState.DOMINATOR_TREE,
                     "If collapsing dominator tree nodes, should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(collapseDominatorTreeNode(selectedSnapshot.id, node));
            },
            onDominatorTreeFocus: node => {
              assert(view === viewState.DOMINATOR_TREE,
                     "If focusing dominator tree nodes, should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(focusDominatorTreeNode(selectedSnapshot.id, node));
            },
            view,
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

module.exports = connect(mapStateToProps)(MemoryApp);
