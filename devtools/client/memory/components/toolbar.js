const { DOM, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

const Toolbar = module.exports = createClass({
  displayName: "toolbar",
  propTypes: {
    breakdowns: PropTypes.arrayOf(PropTypes.shape({
      name: PropTypes.string.isRequired,
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    onTakeSnapshotClick: PropTypes.func.isRequired,
    onBreakdownChange: PropTypes.func.isRequired,
  },

  render() {
    let { onTakeSnapshotClick, onBreakdownChange, breakdowns } = this.props;
    return (
      DOM.div({ className: "devtools-toolbar" }, [
        DOM.button({ className: `take-snapshot devtools-button`, onClick: onTakeSnapshotClick }),
        DOM.select({
          className: `select-breakdown`,
          onChange: e => onBreakdownChange(e.target.value),
        }, breakdowns.map(({ name, displayName }) => DOM.option({ value: name }, displayName)))
      ])
    );
  }
});
