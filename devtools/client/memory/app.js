const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { selectSnapshotAndRefresh, takeSnapshotAndCensus } = require("./actions/snapshot");
const { setBreakdownAndRefresh } = require("./actions/breakdown");
const { breakdownNameToSpec, getBreakdownDisplayData } = require("./utils");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));
const HeapView = createFactory(require("./components/heap"));
const { app: appModel } = require("./models");

const App = createClass({
  displayName: "memory-tool",

  propTypes: appModel,

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
          breakdowns: getBreakdownDisplayData(),
          onTakeSnapshotClick: () => dispatch(takeSnapshotAndCensus(front, heapWorker)),
          onBreakdownChange: breakdown =>
            dispatch(setBreakdownAndRefresh(heapWorker, breakdownNameToSpec(breakdown))),
        }),

        dom.div({ id: "memory-tool-container" }, [
          List({
            itemComponent: SnapshotListItem,
            items: snapshots,
            onClick: snapshot => dispatch(selectSnapshotAndRefresh(heapWorker, snapshot))
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
