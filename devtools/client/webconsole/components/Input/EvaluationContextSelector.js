/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const frameworkActions = require("devtools/client/framework/actions/index");
const webconsoleActions = require("devtools/client/webconsole/actions/index");

const { l10n } = require("devtools/client/webconsole/utils/messages");
const targetSelectors = require("devtools/client/framework/reducers/targets");
const { TARGET_TYPES } = frameworkActions;

// Additional Components
const MenuButton = createFactory(
  require("devtools/client/shared/components/menu/MenuButton")
);

loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});

loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

class EvaluationContextSelector extends Component {
  static get propTypes() {
    return {
      selectTarget: PropTypes.func.isRequired,
      updateInstantEvaluationResultForCurrentExpression:
        PropTypes.func.isRequired,
      selectedTarget: PropTypes.object,
      targets: PropTypes.array,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  componentDidUpdate(prevProps) {
    if (this.props.selectedTarget !== prevProps.selectedTarget) {
      this.props.updateInstantEvaluationResultForCurrentExpression();
    }
  }

  renderMenuItem(target) {
    const { selectTarget, selectedTarget } = this.props;

    const label =
      target.type == TARGET_TYPES.MAIN_TARGET
        ? l10n.getStr("webconsole.input.selector.top")
        : target.name;

    return MenuItem({
      key: `webconsole-evaluation-selector-item-${target.actorID}`,
      className: "menu-item webconsole-evaluation-selector-item",
      type: "checkbox",
      checked: selectedTarget
        ? selectedTarget == target
        : target.type == TARGET_TYPES.MAIN_TARGET,
      label,
      tooltip: target.url,
      onClick: () => selectTarget(target.actorID),
    });
  }

  renderMenuItems() {
    const { targets } = this.props;

    let mainTarget;
    const contentProcesses = [];
    const workers = [];

    for (const target of targets) {
      const menuItem = this.renderMenuItem(target);

      if (target.type == TARGET_TYPES.MAIN_TARGET) {
        mainTarget = menuItem;
      } else if (target.type == TARGET_TYPES.CONTENT_PROCESS) {
        contentProcesses.push(menuItem);
      } else if (target.type == TARGET_TYPES.WORKER) {
        workers.push(menuItem);
      }
    }

    const items = [mainTarget];

    if (contentProcesses.length > 0) {
      items.push(
        MenuItem({ role: "menuseparator", key: "process-separator" }),
        ...contentProcesses
      );
    }

    if (workers.length > 0) {
      items.push(
        MenuItem({ role: "menuseparator", key: "worker-separator" }),
        ...workers
      );
    }

    return MenuList({ id: "webconsole-console-settings-menu-list" }, items);
  }

  getLabel() {
    const { selectedTarget } = this.props;

    if (!selectedTarget || selectedTarget.type == TARGET_TYPES.MAIN_TARGET) {
      return l10n.getStr("webconsole.input.selector.top");
    }

    return selectedTarget.name;
  }

  render() {
    const { webConsoleUI, targets, selectedTarget } = this.props;
    const doc = webConsoleUI.document;
    const { toolbox } = webConsoleUI.wrapper;

    if (targets.length <= 1) {
      return null;
    }

    return MenuButton(
      {
        menuId: "webconsole-input-evaluationsButton",
        toolboxDoc: toolbox ? toolbox.doc : doc,
        label: this.getLabel(),
        className:
          "webconsole-evaluation-selector-button devtools-button devtools-dropdown-button" +
          (selectedTarget && selectedTarget.type !== TARGET_TYPES.MAIN_TARGET
            ? " webconsole-evaluation-selector-button-non-top"
            : ""),
        title: l10n.getStr("webconsole.input.selector.tooltip"),
      },
      // We pass the children in a function so we don't require the MenuItem and MenuList
      // components until we need to display them (i.e. when the button is clicked).
      () => this.renderMenuItems()
    );
  }
}

const toolboxConnected = connect(
  state => ({
    targets: targetSelectors.getToolboxTargets(state),
    selectedTarget: targetSelectors.getSelectedTarget(state),
  }),
  dispatch => ({
    selectTarget: actorID => dispatch(frameworkActions.selectTarget(actorID)),
  }),
  undefined,
  { storeKey: "toolbox-store" }
)(EvaluationContextSelector);

module.exports = connect(
  state => state,
  dispatch => ({
    updateInstantEvaluationResultForCurrentExpression: () =>
      dispatch(
        webconsoleActions.updateInstantEvaluationResultForCurrentExpression()
      ),
  })
)(toolboxConnected);
