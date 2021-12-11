/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/memory/utils");
const models = require("devtools/client/memory/models");
const { viewState } = require("devtools/client/memory/constants");

class Toolbar extends Component {
  static get propTypes() {
    return {
      censusDisplays: PropTypes.arrayOf(models.censusDisplay).isRequired,
      censusDisplay: models.censusDisplay.isRequired,
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
      labelDisplays: PropTypes.arrayOf(models.labelDisplay).isRequired,
      labelDisplay: models.labelDisplay.isRequired,
      onLabelDisplayChange: PropTypes.func.isRequired,
      treeMapDisplays: PropTypes.arrayOf(models.treeMapDisplay).isRequired,
      onTreeMapDisplayChange: PropTypes.func.isRequired,
      snapshots: PropTypes.arrayOf(models.snapshot).isRequired,
    };
  }

  render() {
    const {
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
          className: "toolbar-group",
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
              className: "devtools-toolbar-select select-display",
              onChange: e => {
                const newDisplay = censusDisplays.find(
                  b => b.displayName === e.target.value
                );
                onCensusDisplayChange(newDisplay);
              },
              value: censusDisplay.displayName,
            },
            censusDisplays.map(({ tooltip, displayName }) =>
              dom.option(
                {
                  key: `display-${displayName}`,
                  value: displayName,
                  title: tooltip,
                },
                displayName
              )
            )
          )
        ),

        dom.span({ className: "devtools-separator" }),

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
      assert(
        treeMapDisplays.length >= 1,
        "Should always have at least one tree map display"
      );

      // Only show the dropdown if there are multiple display options
      viewToolbarOptions =
        treeMapDisplays.length > 1
          ? dom.div(
              {
                className: "toolbar-group",
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
                      const newDisplay = treeMapDisplays.find(
                        b => b.displayName === e.target.value
                      );
                      onTreeMapDisplayChange(newDisplay);
                    },
                  },
                  treeMapDisplays.map(({ tooltip, displayName }) =>
                    dom.option(
                      {
                        key: `tree-map-display-${displayName}`,
                        value: displayName,
                        title: tooltip,
                      },
                      displayName
                    )
                  )
                )
              )
            )
          : null;
    } else {
      assert(
        view.state === viewState.DOMINATOR_TREE ||
          view.state === viewState.INDIVIDUALS
      );

      viewToolbarOptions = dom.div(
        {
          className: "toolbar-group",
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
              className: "devtools-toolbar-select select-label-display",
              onChange: e => {
                const newDisplay = labelDisplays.find(
                  b => b.displayName === e.target.value
                );
                onLabelDisplayChange(newDisplay);
              },
              value: labelDisplay.displayName,
            },
            labelDisplays.map(({ tooltip, displayName }) =>
              dom.option(
                {
                  key: `label-display-${displayName}`,
                  value: displayName,
                  title: tooltip,
                },
                displayName
              )
            )
          )
        )
      );
    }

    let viewSelect;
    if (
      view.state !== viewState.DIFFING &&
      view.state !== viewState.INDIVIDUALS
    ) {
      viewSelect = dom.label(
        {
          title: L10N.getStr("toolbar.view.tooltip"),
        },
        L10N.getStr("toolbar.view"),
        dom.select(
          {
            id: "select-view",
            className: "devtools-toolbar-select select-view",
            onChange: e => onViewChange(e.target.value),
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

    return dom.div(
      {
        className: "devtools-toolbar",
      },

      dom.div(
        {
          className: "toolbar-group",
        },

        dom.button({
          id: "clear-snapshots",
          className: "clear-snapshots devtools-button",
          disabled: !snapshots.length,
          onClick: onClearSnapshotsClick,
          title: L10N.getStr("clear-snapshots.tooltip"),
        }),

        dom.button({
          id: "take-snapshot",
          className: "take-snapshot devtools-button",
          onClick: onTakeSnapshotClick,
          title: L10N.getStr("take-snapshot"),
        }),

        dom.button({
          id: "diff-snapshots",
          className:
            "devtools-button devtools-monospace" + (diffing ? " checked" : ""),
          disabled: snapshots.length < 2,
          onClick: onToggleDiffing,
          title: L10N.getStr("diff-snapshots.tooltip"),
        }),

        dom.button({
          id: "import-snapshot",
          className: "import-snapshot devtools-button",
          onClick: onImportClick,
          title: L10N.getStr("import-snapshot"),
        })
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
    );
  }
}

module.exports = Toolbar;
