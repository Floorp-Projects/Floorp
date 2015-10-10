const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { selectSnapshot, takeSnapshot } = require("./actions/snapshot");
const Toolbar = createFactory(require("./components/toolbar"));
const List = createFactory(require("./components/list"));
const SnapshotListItem = createFactory(require("./components/snapshot-list-item"));

const stateModel = {
  /**
   * {MemoryFront}
   * Used to communicate with the platform.
   */
  front: PropTypes.any,

  /**
   * {Array<Snapshot>}
   * List of references to all snapshots taken
   */
  snapshots: PropTypes.arrayOf(PropTypes.shape({
    id: PropTypes.number.isRequired,
    snapshotId: PropTypes.string,
    selected: PropTypes.bool.isRequired,
    status: PropTypes.oneOf([
      "start",
      "done",
      "error",
    ]).isRequired,
  }))
};


const App = createClass({
  displayName: "memory-tool",

  propTypes: stateModel,

  childContextTypes: {
    front: PropTypes.any,
  },

  getChildContext() {
    return {
      front: this.props.front,
    }
  },

  render() {
    let { dispatch, snapshots, front } = this.props;
    return (
      dom.div({ id: "memory-tool" }, [

        Toolbar({
          buttons: [{
            className: "take-snapshot",
            onClick: () => dispatch(takeSnapshot(front))
          }]
        }),

        List({
          itemComponent: SnapshotListItem,
          items: snapshots,
          onClick: snapshot => dispatch(selectSnapshot(snapshot))
        })
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
