const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSnapshotStatusText } = require("../utils");
const { snapshotState: states } = require("../constants");
const TAKE_SNAPSHOT_TEXT = "Take snapshot";

/**
 * Main view for the memory tool -- contains several panels for different states;
 * an initial state of only a button to take a snapshot, loading states, and the
 * heap view tree.
 */

const Heap = module.exports = createClass({
  displayName: "heap-view",

  propTypes: {
    onSnapshotClick: PropTypes.func.isRequired,
    snapshot: PropTypes.any,
  },

  render() {
    let { snapshot, onSnapshotClick } = this.props;
    let pane;
    let census = snapshot ? snapshot.census : null;
    let state = snapshot ? snapshot.state : "initial";
    let statusText = getSnapshotStatusText(snapshot);

    switch (state) {
      case "initial":
        pane = dom.div({ className: "heap-view-panel", "data-state": "initial" },
          dom.button({ className: "take-snapshot", onClick: onSnapshotClick }, TAKE_SNAPSHOT_TEXT)
        );
        break;
      case states.SAVING:
      case states.SAVED:
      case states.READING:
      case states.READ:
      case states.SAVING_CENSUS:
        pane = dom.div({ className: "heap-view-panel", "data-state": state }, statusText);
        break;
      case states.SAVED_CENSUS:
        pane = dom.div({ className: "heap-view-panel", "data-state": "loaded" }, JSON.stringify(census || {}));
        break;
    }

    return (
      dom.div({ id: "heap-view", "data-state": state }, pane)
    )
  }
});
