/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

const models = require("../models");

const Toolbar = module.exports = createClass({
  displayName: "toolbar",
  propTypes: {
    breakdowns: PropTypes.arrayOf(PropTypes.shape({
      name: PropTypes.string.isRequired,
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    onTakeSnapshotClick: PropTypes.func.isRequired,
    onBreakdownChange: PropTypes.func.isRequired,
    onToggleRecordAllocationStacks: PropTypes.func.isRequired,
    allocations: models.allocations
  },

  render() {
    let {
      onTakeSnapshotClick,
      onBreakdownChange,
      breakdowns,
      onToggleRecordAllocationStacks,
      allocations,
    } = this.props;

    return (
      DOM.div({ className: "devtools-toolbar" }, [
        DOM.button({ className: `take-snapshot devtools-button`, onClick: onTakeSnapshotClick }),

        DOM.select({
          className: `select-breakdown`,
          onChange: e => onBreakdownChange(e.target.value),
        }, breakdowns.map(({ name, displayName }) => DOM.option({ value: name }, displayName))),

        DOM.label({}, [
          DOM.input({
            type: "checkbox",
            checked: allocations.recording,
            disabled: allocations.togglingInProgress,
            onChange: onToggleRecordAllocationStacks,
          }),
          // TODO bug 1214799
          "Record allocation stacks"
        ])
      ])
    );
  }
});
