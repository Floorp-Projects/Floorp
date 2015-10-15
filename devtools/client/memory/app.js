const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { selectSnapshot, takeSnapshotAndCensus } = require("./actions/snapshot");
const { snapshotState } = require("./constants");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));
const HeapView = createFactory(require("./components/heap"));

const stateModel = {
  /**
   * {MemoryFront}
   * Used to communicate with the platform.
   */
  front: PropTypes.any,

  /**
   * {HeapAnalysesClient}
   * Used to communicate with the worker that performs analyses on heaps.
   */
  heapWorker: PropTypes.any,

  /**
   * The breakdown object DSL describing how we want
   * the census data to be.
   * @see `js/src/doc/Debugger/Debugger.Memory.md`
   */
  breakdown: PropTypes.object.isRequired,

  /**
   * {Array<Snapshot>}
   * List of references to all snapshots taken
   */
  snapshots: PropTypes.arrayOf(PropTypes.shape({
    // Unique ID for a snapshot
    id: PropTypes.number.isRequired,
    // fs path to where the snapshot is stored; used to
    // identify the snapshot for HeapAnalysesClient.
    path: PropTypes.string,
    // Whether or not this snapshot is currently selected.
    selected: PropTypes.bool.isRequired,
    // Whther or not the snapshot has been read into memory.
    // Only needed to do once.
    snapshotRead: PropTypes.bool.isRequired,
    // State the snapshot is in
    // @see ./constants.js
    state: PropTypes.oneOf(Object.keys(snapshotState)).isRequired,
    // Data of a census breakdown
    census: PropTypes.any,
  }))
};

const App = createClass({
  displayName: "memory-tool",

  propTypes: stateModel,

  childContextTypes: {
    front: PropTypes.any,
    heapWorker: PropTypes.any,
  },

  getChildContext() {
    return {
      front: this.props.front,
      heapWorker: this.props.heapWorker,
    }
  },

  render() {
    let { dispatch, snapshots, front, heapWorker, breakdown } = this.props;
    let selectedSnapshot = snapshots.find(s => s.selected);

    return (
      dom.div({ id: "memory-tool" }, [

        Toolbar({
          buttons: [{
            className: "take-snapshot",
            onClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker))
          }]
        }),

        dom.div({ id: "memory-tool-container" }, [
          List({
            itemComponent: SnapshotListItem,
            items: snapshots,
            onClick: snapshot => dispatch(selectSnapshot(snapshot))
          }),

          HeapView({
            snapshot: selectedSnapshot,
            onSnapshotClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker))
          }),
        ])
      ])
    );
  },
});

/**
 * Passed into react-redux's `connect` method that is called on store change
 * and passed to components.
 */
function mapStateToProps (state) {
  return { snapshots: state.snapshots };
}

module.exports = connect(mapStateToProps)(App);
