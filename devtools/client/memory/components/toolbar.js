/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const models = require("../models");
const { viewState } = require("../constants");

module.exports = createClass({
  displayName: "Toolbar",

  propTypes: {
    censusDisplays: PropTypes.arrayOf(PropTypes.shape({
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    censusDisplay: PropTypes.shape({
      displayName: PropTypes.string.isRequired,
    }).isRequired,
    onTakeSnapshotClick: PropTypes.func.isRequired,
    onImportClick: PropTypes.func.isRequired,
    onClearSnapshotsClick: PropTypes.func.isRequired,
    onCensusDisplayChange: PropTypes.func.isRequired,
    onToggleRecordAllocationStacks: PropTypes.func.isRequired,
    allocations: models.allocations,
    filterString: PropTypes.string,
    setFilterString: PropTypes.func.isRequired,
    diffing: models.diffingModel,
    onToggleDiffing: PropTypes.func.isRequired,
    view: models.view.isRequired,
    onViewChange: PropTypes.func.isRequired,
    labelDisplays: PropTypes.arrayOf(PropTypes.shape({
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    labelDisplay: PropTypes.shape({
      displayName: PropTypes.string.isRequired,
    }).isRequired,
    onLabelDisplayChange: PropTypes.func.isRequired,
    treeMapDisplays: PropTypes.arrayOf(PropTypes.shape({
      displayName: PropTypes.string.isRequired,
    })).isRequired,
    onTreeMapDisplayChange: PropTypes.func.isRequired,
    snapshots: PropTypes.arrayOf(models.snapshot).isRequired,
  },

  render() {
    let {
      onTakeSnapshotClick,
      onImportClick,
      onClearSnapshotsClick,
      onCensusDisplayChange,
      censusDisplays,
      censusDisplay,
      labelDisplays,
      labelDisplay,
      onLabelDisplayChange,
      treeMapDisplays,
      onTreeMapDisplayChange,
      onToggleRecordAllocationStacks,
      allocations,
      filterString,
      setFilterString,
      snapshots,
      diffing,
      onToggleDiffing,
      view,
      onViewChange,
    } = this.props;

    let viewToolbarOptions;
    if (view.state == viewState.CENSUS || view.state === viewState.DIFFING) {
      viewToolbarOptions = dom.div(
        {
          className: "toolbar-group"
        },

        dom.label(
          {
            className: "display-by",
            title: L10N.getStr("toolbar.displayBy.tooltip"),
          },
          L10N.getStr("toolbar.displayBy"),
          dom.select(
            {
              id: "select-display",
              className: "select-display",
              onChange: e => {
                const newDisplay =
                  censusDisplays.find(b => b.displayName === e.target.value);
                onCensusDisplayChange(newDisplay);
              },
              value: censusDisplay.displayName,
            },
            censusDisplays.map(({ tooltip, displayName }) => dom.option(
              {
                key: `display-${displayName}`,
                value: displayName,
                title: tooltip,
              },
              displayName
            ))
          )
        ),

        dom.div({ id: "toolbar-spacer", className: "spacer" }),

        dom.input({
          id: "filter",
          type: "search",
          className: "devtools-filterinput",
          placeholder: L10N.getStr("filter.placeholder"),
          title: L10N.getStr("filter.tooltip"),
          onChange: event => setFilterString(event.target.value),
          value: filterString || undefined,
        })
      );
    } else if (view.state == viewState.TREE_MAP) {
      assert(treeMapDisplays.length >= 1,
       "Should always have at least one tree map display");

      // Only show the dropdown if there are multiple display options
      viewToolbarOptions = treeMapDisplays.length > 1
        ? dom.div(
          {
            className: "toolbar-group"
          },

            dom.label(
              {
                className: "display-by",
                title: L10N.getStr("toolbar.displayBy.tooltip"),
              },
              L10N.getStr("toolbar.displayBy"),
              dom.select(
                {
                  id: "select-tree-map-display",
                  onChange: e => {
                    const newDisplay =
                      treeMapDisplays.find(b => b.displayName === e.target.value);
                    onTreeMapDisplayChange(newDisplay);
                  },
                },
                treeMapDisplays.map(({ tooltip, displayName }) => dom.option(
                  {
                    key: `tree-map-display-${displayName}`,
                    value: displayName,
                    title: tooltip,
                  },
                  displayName
                ))
              )
            )
          )
        : null;
    } else {
      assert(view.state === viewState.DOMINATOR_TREE ||
             view.state === viewState.INDIVIDUALS);

      viewToolbarOptions = dom.div(
        {
          className: "toolbar-group"
        },

        dom.label(
          {
            className: "label-by",
            title: L10N.getStr("toolbar.labelBy.tooltip"),
          },
          L10N.getStr("toolbar.labelBy"),
          dom.select(
            {
              id: "select-label-display",
              onChange: e => {
                const newDisplay =
                  labelDisplays.find(b => b.displayName === e.target.value);
                onLabelDisplayChange(newDisplay);
              },
              value: labelDisplay.displayName,
            },
            labelDisplays.map(({ tooltip, displayName }) => dom.option(
              {
                key: `label-display-${displayName}`,
                value: displayName,
                title: tooltip,
              },
              displayName
            ))
          )
        )
      );
    }

    let viewSelect;
    if (view.state !== viewState.DIFFING && view.state !== viewState.INDIVIDUALS) {
      viewSelect = dom.label(
        {
          title: L10N.getStr("toolbar.view.tooltip"),
        },
        L10N.getStr("toolbar.view"),
        dom.select(
          {
            id: "select-view",
            onChange: e => onViewChange(e.target.value),
            defaultValue: view,
            value: view.state,
          },
          dom.option(
            {
              value: viewState.TREE_MAP,
              title: L10N.getStr("toolbar.view.treemap.tooltip"),
            },
            L10N.getStr("toolbar.view.treemap")
          ),
          dom.option(
            {
              value: viewState.CENSUS,
              title: L10N.getStr("toolbar.view.census.tooltip"),
            },
            L10N.getStr("toolbar.view.census")
          ),
          dom.option(
            {
              value: viewState.DOMINATOR_TREE,
              title: L10N.getStr("toolbar.view.dominators.tooltip"),
            },
            L10N.getStr("toolbar.view.dominators")
          )
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
            id: "clear-snapshots",
            className: "clear-snapshots devtools-button",
            onClick: onClearSnapshotsClick,
            title: L10N.getStr("clear-snapshots.tooltip")
          }),

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
            }
          ),

          dom.button(
            {
              id: "import-snapshot",
              className: "devtools-toolbarbutton import-snapshot devtools-button",
              onClick: onImportClick,
              title: L10N.getStr("import-snapshot"),
            }
          )
        ),

        dom.label(
          {
            id: "record-allocation-stacks-label",
            title: L10N.getStr("checkbox.recordAllocationStacks.tooltip"),
          },
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
