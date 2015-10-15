const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSnapshotStatusText } = require("../utils");

const SnapshotListItem = module.exports = createClass({
  displayName: "snapshot-list-item",

  propTypes: {
    onClick: PropTypes.func,
    item: PropTypes.any.isRequired,
    index: PropTypes.number.isRequired,
  },

  render() {
    let { index, item, onClick } = this.props;
    let className = `snapshot-list-item ${item.selected ? " selected" : ""}`;
    let statusText = getSnapshotStatusText(item);

    return (
      dom.li({ className, onClick },
        dom.span({
          className: `snapshot-title ${statusText ? " devtools-throbber" : ""}`
        }, `Snapshot #${index}`),

        statusText ? dom.span({ className: "snapshot-state" }, statusText) : void 0
      )
    );
  }
});

