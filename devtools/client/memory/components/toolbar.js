/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { assert } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const models = require("../models");
const { viewState } = require("../constants");

const Toolbar = module.exports = createClass({
  displayName: "Toolbar",
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
    view: PropTypes.string.isRequired,
    onViewChange: PropTypes.func.isRequired,
    dominatorTreeBreakdowns: PropTypes.arrayOf(PropTypes.shape({
      name: PropTypes.string.isRequired,
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    onDominatorTreeBreakdownChange: PropTypes.func.isRequired,
    snapshots: PropTypes.arrayOf(models.snapshot).isRequired,
  },

  render() {
    let {
      onTakeSnapshotClick,
      onImportClick,
      onBreakdownChange,
      breakdowns,
      dominatorTreeBreakdowns,
      onDominatorTreeBreakdownChange,
      onToggleRecordAllocationStacks,
      allocations,
      onToggleInverted,
      inverted,
      filterString,
      setFilterString,
      snapshots,
      diffing,
      onToggleDiffing,
      view,
      onViewChange,
    } = this.props;

    let viewToolbarOptions;
    if (view == viewState.CENSUS || view === viewState.DIFFING) {
      viewToolbarOptions = dom.div(
        {
          className: "toolbar-group"
        },

        dom.label(
          { className: "breakdown-by" },
          L10N.getStr("toolbar.breakdownBy"),
          dom.select(
            {
              id: "select-breakdown",
              className: "select-breakdown",
              onChange: e => onBreakdownChange(e.target.value),
            },
            breakdowns.map(({ name, displayName }) => dom.option(
              {
                key: name,
                value: name
              },
              displayName
            ))
          )
        ),

        dom.label(
          {},
          dom.input({
            id: "invert-tree-checkbox",
            type: "checkbox",
            checked: inverted,
            onChange: onToggleInverted,
          }),
          L10N.getStr("checkbox.invertTree")
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
      );
    } else {
      assert(view === viewState.DOMINATOR_TREE);

      viewToolbarOptions = dom.div(
        {
          className: "toolbar-group"
        },

        dom.label(
          { className: "label-by" },
          L10N.getStr("toolbar.labelBy"),
          dom.select(
            {
              id: "select-dominator-tree-breakdown",
              onChange: e => onDominatorTreeBreakdownChange(e.target.value),
            },
            dominatorTreeBreakdowns.map(({ name, displayName }) => dom.option(
              {
                key: name,
                value: name
              },
              displayName
            ))
          )
        )
      );
    }

    let viewSelect;
    if (view !== viewState.DIFFING) {
      viewSelect = dom.label(
        {},
        L10N.getStr("toolbar.view"),
        dom.select(
          {
            id: "select-view",
            onChange: e => onViewChange(e.target.value),
            defaultValue: viewState.CENSUS,
          },
          dom.option({ value: viewState.CENSUS },
                     L10N.getStr("toolbar.view.census")),
          dom.option({ value: viewState.DOMINATOR_TREE },
                     L10N.getStr("toolbar.view.dominators"))
        )
      );
    }

    return (
      dom.div(
        {
          className: "devtools-toolbar"
        },

        dom.div(
          {
            className: "toolbar-group"
          },

          dom.button({
            id: "take-snapshot",
            className: "take-snapshot devtools-button",
            onClick: onTakeSnapshotClick,
            title: L10N.getStr("take-snapshot")
          }),

          dom.button(
            {
              id: "diff-snapshots",
              className: "devtools-button devtools-monospace" + (!!diffing ? " checked" : ""),
              disabled: snapshots.length < 2,
              onClick: onToggleDiffing,
              title: L10N.getStr("diff-snapshots.tooltip"),
            },
            L10N.getStr("diff-snapshots")
          ),

          dom.button(
            {
              id: "import-snapshot",
              className: "devtools-toolbarbutton import-snapshot devtools-button",
              onClick: onImportClick,
              title: L10N.getStr("import-snapshot"),
              "data-text-only": true,
            },
            L10N.getStr("import-snapshot")
          )
        ),

        dom.label(
          {},
          dom.input({
            id: "record-allocation-stacks-checkbox",
            type: "checkbox",
            checked: allocations.recording,
            disabled: allocations.togglingInProgress,
            onChange: onToggleRecordAllocationStacks,
          }),
          L10N.getStr("checkbox.recordAllocationStacks")
        ),

        viewSelect,
        viewToolbarOptions
      )
    );
  }
});
