/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { appinfo } = require("Services");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { censusDisplays, labelDisplays, treeMapDisplays, diffingState, viewState } = require("./constants");
const { toggleRecordingAllocationStacks } = require("./actions/allocations");
const { setCensusDisplayAndRefresh } = require("./actions/census-display");
const { setLabelDisplayAndRefresh } = require("./actions/label-display");
const { setTreeMapDisplayAndRefresh } = require("./actions/tree-map-display");

const {
  getCustomCensusDisplays,
  getCustomLabelDisplays,
  getCustomTreeMapDisplays,
} = require("devtools/client/memory/utils");
const {
  selectSnapshotForDiffingAndRefresh,
  toggleDiffing,
  expandDiffingCensusNode,
  collapseDiffingCensusNode,
  focusDiffingCensusNode,
} = require("./actions/diffing");
const { setFilterStringAndRefresh } = require("./actions/filter");
const { pickFileAndExportSnapshot, pickFileAndImportSnapshotAndCensus } = require("./actions/io");
const {
  selectSnapshotAndRefresh,
  takeSnapshotAndCensus,
  clearSnapshots,
  deleteSnapshot,
  fetchImmediatelyDominated,
  expandCensusNode,
  collapseCensusNode,
  focusCensusNode,
  expandDominatorTreeNode,
  collapseDominatorTreeNode,
  focusDominatorTreeNode,
  fetchIndividuals,
  focusIndividual,
} = require("./actions/snapshot");
const { changeViewAndRefresh, popViewAndRefresh } = require("./actions/view");
const { resizeShortestPaths } = require("./actions/sizes");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));
const Heap = createFactory(require("./components/heap"));
const { app: appModel } = require("./models");

const MemoryApp = createClass({
  displayName: "MemoryApp",

  propTypes: appModel,

  childContextTypes: {
    front: PropTypes.any,
    heapWorker: PropTypes.any,
    toolbox: PropTypes.any,
  },

  getDefaultProps() {
    return {};
  },

  getChildContext() {
    return {
      front: this.props.front,
      heapWorker: this.props.heapWorker,
      toolbox: this.props.toolbox,
    };
  },

  componentDidMount() {
    // Attach the keydown listener directly to the window. When an element that
    // has the focus (such as a tree node) is removed from the DOM, the focus
    // falls back to the body.
    window.addEventListener("keydown", this.onKeyDown);
  },

  componentWillUnmount() {
    window.removeEventListener("keydown", this.onKeyDown);
  },

  onKeyDown(e) {
    let { snapshots, dispatch, heapWorker } = this.props;
    const selectedSnapshot = snapshots.find(s => s.selected);
    const selectedIndex = snapshots.indexOf(selectedSnapshot);

    let isOSX = appinfo.OS == "Darwin";
    let isAccelKey = (isOSX && e.metaKey) || (!isOSX && e.ctrlKey);

    // On ACCEL+UP, select previous snapshot.
    if (isAccelKey && e.key === "ArrowUp") {
      let previousIndex = Math.max(0, selectedIndex - 1);
      let previousSnapshotId = snapshots[previousIndex].id;
      dispatch(selectSnapshotAndRefresh(heapWorker, previousSnapshotId));
    }

    // On ACCEL+DOWN, select next snapshot.
    if (isAccelKey && e.key === "ArrowDown") {
      let nextIndex = Math.min(snapshots.length - 1, selectedIndex + 1);
      let nextSnapshotId = snapshots[nextIndex].id;
      dispatch(selectSnapshotAndRefresh(heapWorker, nextSnapshotId));
    }
  },

  _getCensusDisplays() {
    const customDisplays = getCustomCensusDisplays();
    const custom = Object.keys(customDisplays).reduce((arr, key) => {
      arr.push(customDisplays[key]);
      return arr;
    }, []);

    return [
      censusDisplays.coarseType,
      censusDisplays.allocationStack,
      censusDisplays.invertedAllocationStack,
    ].concat(custom);
  },

  _getLabelDisplays() {
    const customDisplays = getCustomLabelDisplays();
    const custom = Object.keys(customDisplays).reduce((arr, key) => {
      arr.push(customDisplays[key]);
      return arr;
    }, []);

    return [
      labelDisplays.coarseType,
      labelDisplays.allocationStack,
    ].concat(custom);
  },

  _getTreeMapDisplays() {
    const customDisplays = getCustomTreeMapDisplays();
    const custom = Object.keys(customDisplays).reduce((arr, key) => {
      arr.push(customDisplays[key]);
      return arr;
    }, []);

    return [
      treeMapDisplays.coarseType
    ].concat(custom);
  },

  render() {
    let {
      dispatch,
      snapshots,
      front,
      heapWorker,
      allocations,
      toolbox,
      filter,
      diffing,
      view,
      sizes,
      censusDisplay,
      labelDisplay,
      individuals,
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
          censusDisplays: this._getCensusDisplays(),
          censusDisplay,
          onCensusDisplayChange: newDisplay =>
            dispatch(setCensusDisplayAndRefresh(heapWorker, newDisplay)),
          onImportClick: () => dispatch(pickFileAndImportSnapshotAndCensus(heapWorker)),
          onClearSnapshotsClick: () => dispatch(clearSnapshots(heapWorker)),
          onTakeSnapshotClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker)),
          onToggleRecordAllocationStacks: () =>
            dispatch(toggleRecordingAllocationStacks(front)),
          allocations,
          filterString: filter,
          setFilterString: filterString =>
            dispatch(setFilterStringAndRefresh(filterString, heapWorker)),
          diffing,
          onToggleDiffing: () => dispatch(toggleDiffing()),
          view,
          labelDisplays: this._getLabelDisplays(),
          labelDisplay,
          onLabelDisplayChange: newDisplay =>
            dispatch(setLabelDisplayAndRefresh(heapWorker, newDisplay)),
          treeMapDisplays: this._getTreeMapDisplays(),
          onTreeMapDisplayChange: newDisplay =>
            dispatch(setTreeMapDisplayAndRefresh(heapWorker, newDisplay)),
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
            onDelete: snapshot => dispatch(deleteSnapshot(heapWorker, snapshot)),
            onClick: onClickSnapshotListItem,
            diffing,
          }),

          Heap({
            snapshot: selectedSnapshot,
            diffing,
            onViewSourceInDebugger: frame =>
              toolbox.viewSourceInDebugger(frame.source, frame.line),
            onSnapshotClick: () =>
              dispatch(takeSnapshotAndCensus(front, heapWorker)),
            onLoadMoreSiblings: lazyChildren =>
              dispatch(fetchImmediatelyDominated(heapWorker,
                                                 selectedSnapshot.id,
                                                 lazyChildren)),
            onPopView: () => dispatch(popViewAndRefresh(heapWorker)),
            individuals,
            onViewIndividuals: node => {
              const snapshotId = diffing
                ? diffing.secondSnapshotId
                : selectedSnapshot.id;
              dispatch(fetchIndividuals(heapWorker,
                                        snapshotId,
                                        censusDisplay.breakdown,
                                        node.reportLeafIndex));
            },
            onFocusIndividual: node => {
              assert(view.state === viewState.INDIVIDUALS,
                     "Should be in the individuals view");
              dispatch(focusIndividual(node));
            },
            onCensusExpand: (census, node) => {
              if (diffing) {
                assert(diffing.census === census,
                       "Should only expand active census");
                dispatch(expandDiffingCensusNode(node));
              } else {
                assert(selectedSnapshot && selectedSnapshot.census === census,
                       "If not diffing, " +
                       "should be expanding on selected snapshot's census");
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
                       "If not diffing, " +
                       "should be collapsing on selected snapshot's census");
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
                       "If not diffing, " +
                       "should be focusing on nodes in selected snapshot's census");
                dispatch(focusCensusNode(selectedSnapshot.id, node));
              }
            },
            onDominatorTreeExpand: node => {
              assert(view.state === viewState.DOMINATOR_TREE,
                     "If expanding dominator tree nodes, " +
                     "should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(expandDominatorTreeNode(selectedSnapshot.id, node));
            },
            onDominatorTreeCollapse: node => {
              assert(view.state === viewState.DOMINATOR_TREE,
                     "If collapsing dominator tree nodes, " +
                     "should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(collapseDominatorTreeNode(selectedSnapshot.id, node));
            },
            onDominatorTreeFocus: node => {
              assert(view.state === viewState.DOMINATOR_TREE,
                     "If focusing dominator tree nodes, " +
                     "should be in dominator tree view");
              assert(selectedSnapshot, "...and we should have a selected snapshot");
              assert(selectedSnapshot.dominatorTree,
                     "...and that snapshot should have a dominator tree");
              dispatch(focusDominatorTreeNode(selectedSnapshot.id, node));
            },
            onShortestPathsResize: newSize => {
              dispatch(resizeShortestPaths(newSize));
            },
            sizes,
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
function mapStateToProps(state) {
  return state;
}

module.exports = connect(mapStateToProps)(MemoryApp);
