/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const targetActions = require("devtools/shared/commands/target/actions/targets");
const webconsoleActions = require("devtools/client/webconsole/actions/index");

const { l10n } = require("devtools/client/webconsole/utils/messages");
const targetSelectors = require("devtools/shared/commands/target/selectors/targets");

loader.lazyGetter(this, "TARGET_TYPES", function() {
  return require("devtools/shared/commands/target/target-command").TYPES;
});

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
      lastTargetRefresh: PropTypes.number,
      targets: PropTypes.array,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    if (this.props.selectedTarget !== nextProps.selectedTarget) {
      return true;
    }

    if (this.props.lastTargetRefresh !== nextProps.lastTargetRefresh) {
      return true;
    }

    if (this.props.targets.length !== nextProps.targets.length) {
      return true;
    }

    for (let i = 0; i < nextProps.targets.length; i++) {
      const target = this.props.targets[i];
      const nextTarget = nextProps.targets[i];
      if (target.url != nextTarget.url || target.name != nextTarget.name) {
        return true;
      }
    }
    return false;
  }

  componentDidUpdate(prevProps) {
    if (this.props.selectedTarget !== prevProps.selectedTarget) {
      this.props.updateInstantEvaluationResultForCurrentExpression();
    }
  }

  getIcon(target) {
    if (target.targetType === TARGET_TYPES.FRAME) {
      return "chrome://devtools/content/debugger/images/globe-small.svg";
    }

    if (
      target.targetType === TARGET_TYPES.WORKER ||
      target.targetType === TARGET_TYPES.SHARED_WORKER ||
      target.targetType === TARGET_TYPES.SERVICE_WORKER
    ) {
      return "chrome://devtools/content/debugger/images/worker.svg";
    }

    if (target.targetType === TARGET_TYPES.PROCESS) {
      return "chrome://devtools/content/debugger/images/window.svg";
    }

    return null;
  }

  renderMenuItem(target) {
    const { selectTarget, selectedTarget } = this.props;

    const label = target.isTopLevel
      ? l10n.getStr("webconsole.input.selector.top")
      : target.name;

    return MenuItem({
      key: `webconsole-evaluation-selector-item-${target.actorID}`,
      className: "menu-item webconsole-evaluation-selector-item",
      type: "checkbox",
      checked: selectedTarget ? selectedTarget == target : target.isTopLevel,
      label,
      tooltip: target.url || target.name,
      icon: this.getIcon(target),
      onClick: () => selectTarget(target.actorID),
    });
  }

  renderMenuItems() {
    const { targets } = this.props;

    // Let's sort the targets (using "numeric" so Content processes are ordered by PID).
    const collator = new Intl.Collator("en", { numeric: true });
    targets.sort((a, b) => collator.compare(a.name, b.name));

    let mainTarget;
    const sections = {
      [TARGET_TYPES.FRAME]: [],
      [TARGET_TYPES.WORKER]: [],
      [TARGET_TYPES.SHARED_WORKER]: [],
      [TARGET_TYPES.SERVICE_WORKER]: [],
    };
    // When in Browser Toolbox, we want to display the process targets with the frames
    // in the same process as a group
    // e.g.
    //     |------------------------------|
    //     | Top                          |
    //     | -----------------------------|
    //     | (pid 1234) priviledgedabout  |
    //     | New Tab                      |
    //     | -----------------------------|
    //     | (pid 5678) web               |
    //     | cnn.com                      |
    //     | -----------------------------|
    //     | RemoteSettingWorker.js       |
    //     |------------------------------|
    //
    // This object will be keyed by PID, and each property will be an object with a
    // `process` property (for the process target item), and a `frames` property (and array
    // for all the frame target items).
    const processes = {};

    const { webConsoleUI } = this.props;
    const handleProcessTargets =
      webConsoleUI.isBrowserConsole || webConsoleUI.isBrowserToolboxConsole;

    for (const target of targets) {
      const menuItem = this.renderMenuItem(target);

      if (target.isTopLevel) {
        mainTarget = menuItem;
      } else if (target.targetType == TARGET_TYPES.PROCESS) {
        if (!processes[target.processID]) {
          processes[target.processID] = { frames: [] };
        }
        processes[target.processID].process = menuItem;
      } else if (
        target.targetType == TARGET_TYPES.FRAME &&
        handleProcessTargets &&
        target.processID
      ) {
        // The associated process target might not have been handled yet, so make sure
        // to create it.
        if (!processes[target.processID]) {
          processes[target.processID] = { frames: [] };
        }
        processes[target.processID].frames.push(menuItem);
      } else {
        sections[target.targetType].push(menuItem);
      }
    }

    // Note that while debugging popups, we might have a small period
    // of time where we don't have any top level target when we reload
    // the original tab
    const items = mainTarget ? [mainTarget] : [];

    // Handle PROCESS targets sections first, as we want to display the associated frames
    // below the process to group them.
    if (processes) {
      for (const [pid, { process, frames }] of Object.entries(processes)) {
        items.push(dom.hr({ role: "menuseparator", key: `${pid}-separator` }));
        if (process) {
          items.push(process);
        }
        if (frames) {
          items.push(...frames);
        }
      }
    }

    for (const [targetType, menuItems] of Object.entries(sections)) {
      if (menuItems.length) {
        items.push(
          dom.hr({ role: "menuseparator", key: `${targetType}-separator` }),
          ...menuItems
        );
      }
    }

    return MenuList(
      { id: "webconsole-console-evaluation-context-selector-menu-list" },
      items
    );
  }

  getLabel() {
    const { selectedTarget } = this.props;

    if (!selectedTarget || selectedTarget.isTopLevel) {
      return l10n.getStr("webconsole.input.selector.top");
    }

    return selectedTarget.name;
  }

  render() {
    const { webConsoleUI, targets, selectedTarget } = this.props;

    // Don't render if there's only one target.
    // Also bail out if the console is being destroyed (where WebConsoleUI.wrapper gets
    // nullified).
    if (targets.length <= 1 || !webConsoleUI.wrapper) {
      return null;
    }

    const doc = webConsoleUI.document;
    const { toolbox } = webConsoleUI.wrapper;

    return MenuButton(
      {
        menuId: "webconsole-input-evaluationsButton",
        toolboxDoc: toolbox ? toolbox.doc : doc,
        label: this.getLabel(),
        className:
          "webconsole-evaluation-selector-button devtools-button devtools-dropdown-button" +
          (selectedTarget && !selectedTarget.isTopLevel ? " checked" : ""),
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
    lastTargetRefresh: targetSelectors.getLastTargetRefresh(state),
  }),
  dispatch => ({
    selectTarget: actorID => dispatch(targetActions.selectTarget(actorID)),
  }),
  undefined,
  { storeKey: "target-store" }
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
