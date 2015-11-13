/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const models = require("../models");

const Toolbar = module.exports = createClass({
  displayName: "toolbar",
  propTypes: {
    breakdowns: PropTypes.arrayOf(PropTypes.shape({
      name: PropTypes.string.isRequired,
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    onTakeSnapshotClick: PropTypes.func.isRequired,
    onImportClick: PropTypes.func.isRequired,
    onBreakdownChange: PropTypes.func.isRequired,
    onToggleRecordAllocationStacks: PropTypes.func.isRequired,
    allocations: models.allocations,
    onToggleInverted: PropTypes.func.isRequired,
    inverted: PropTypes.bool.isRequired,
    filterString: PropTypes.string,
    setFilterString: PropTypes.func.isRequired,
    diffing: models.diffingModel,
    onToggleDiffing: PropTypes.func.isRequired,
  },

  render() {
    let {
      onTakeSnapshotClick,
      onImportClick,
      onBreakdownChange,
      breakdowns,
      onToggleRecordAllocationStacks,
      allocations,
      onToggleInverted,
      inverted,
      filterString,
      setFilterString,
      snapshots,
      diffing,
      onToggleDiffing,
    } = this.props;

    return (
      dom.div({ className: "devtools-toolbar" },
        dom.div({ className: "toolbar-group" },
          dom.button({
            id: "take-snapshot",
            className: "take-snapshot devtools-button",
            onClick: onTakeSnapshotClick,
            title: L10N.getStr("take-snapshot")
          }),

          dom.button({
            id: "diff-snapshots",
            className: "devtools-button devtools-monospace" + (!!diffing ? " checked" : ""),
            disabled: snapshots.length < 2,
            onClick: onToggleDiffing,
            title: L10N.getStr("diff-snapshots.tooltip"),
          }, L10N.getStr("diff-snapshots")),

          dom.button({
            id: "import-snapshot",
            className: "devtools-toolbarbutton import-snapshot devtools-button",
            onClick: onImportClick,
            title: L10N.getStr("import-snapshot"),
            "data-text-only": true,
          }, L10N.getStr("import-snapshot"))
        ),

        dom.div({ className: "toolbar-group" },
          dom.label({ className: "breakdown-by" },
            L10N.getStr("toolbar.breakdownBy"),
            dom.select({
              id: "select-breakdown",
              className: "select-breakdown",
              onChange: e => onBreakdownChange(e.target.value),
            }, ...breakdowns.map(({ name, displayName }) => dom.option({ key: name, value: name }, displayName)))
          ),

          dom.label({},
            dom.input({
              id: "invert-tree-checkbox",
              type: "checkbox",
              checked: inverted,
              onChange: onToggleInverted,
            }),
            L10N.getStr("checkbox.invertTree")
          ),

          dom.label({},
            dom.input({
              id: "record-allocation-stacks-checkbox",
              type: "checkbox",
              checked: allocations.recording,
              disabled: allocations.togglingInProgress,
              onChange: onToggleRecordAllocationStacks,
            }),
            L10N.getStr("checkbox.recordAllocationStacks")
          ),

          dom.div({ id: "toolbar-spacer", className: "spacer" }),

          dom.input({
            id: "filter",
            type: "search",
            className: "devtools-searchinput",
            placeholder: L10N.getStr("filter.placeholder"),
            onChange: event => setFilterString(event.target.value),
            value: !!filterString ? filterString : undefined,
          })
        )
      )
    );
  }
});
