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
const threadSelectors = require("devtools/client/framework/reducers/threads");
const { THREAD_TYPES } = frameworkActions;

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
      selectThread: PropTypes.func.isRequired,
      updateInstantEvaluationResultForCurrentExpression:
        PropTypes.func.isRequired,
      selectedThread: PropTypes.object,
      threads: PropTypes.array,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  componentDidUpdate(prevProps) {
    if (this.props.selectedThread !== prevProps.selectedThread) {
      this.props.updateInstantEvaluationResultForCurrentExpression();
    }
  }

  renderMenuItem(thread) {
    const { selectThread, selectedThread } = this.props;

    const label =
      thread.type == THREAD_TYPES.MAIN_THREAD
        ? l10n.getStr("webconsole.input.selector.top")
        : thread.name;

    return MenuItem({
      key: `webconsole-evaluation-selector-item-${thread.actorID}`,
      className: "menu-item webconsole-evaluation-selector-item",
      type: "checkbox",
      checked: selectedThread
        ? selectedThread == thread
        : thread.type == THREAD_TYPES.MAIN_THREAD,
      label,
      tooltip: thread.url,
      onClick: () => selectThread(thread.actorID),
    });
  }

  renderMenuItems() {
    const { threads } = this.props;

    let mainThread;
    const processes = [];
    const workers = [];

    for (const thread of threads) {
      const menuItem = this.renderMenuItem(thread);

      if (thread.type == THREAD_TYPES.MAIN_THREAD) {
        mainThread = menuItem;
      } else if (thread.type == THREAD_TYPES.CONTENT_PROCESS) {
        processes.push(menuItem);
      } else if (thread.type == THREAD_TYPES.WORKER) {
        workers.push(menuItem);
      }
    }

    const items = [mainThread];

    if (processes.length > 0) {
      items.push(
        MenuItem({ role: "menuseparator", key: "process-separator" }),
        ...processes
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
    const { selectedThread } = this.props;

    if (!selectedThread || selectedThread.type == THREAD_TYPES.MAIN_THREAD) {
      return l10n.getStr("webconsole.input.selector.top");
    }

    return selectedThread.name;
  }

  render() {
    const { webConsoleUI, threads, selectedThread } = this.props;
    const doc = webConsoleUI.document;
    const toolbox = webConsoleUI.wrapper.toolbox;

    if (threads.length <= 1) {
      return null;
    }

    return MenuButton(
      {
        menuId: "webconsole-input-evaluationsButton",
        toolboxDoc: toolbox ? toolbox.doc : doc,
        label: this.getLabel(),
        className:
          "webconsole-evaluation-selector-button devtools-button devtools-dropdown-button" +
          (selectedThread && selectedThread.type !== THREAD_TYPES.MAIN_THREAD
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
    threads: threadSelectors.getToolboxThreads(state),
    selectedThread: threadSelectors.getSelectedThread(state),
  }),
  dispatch => ({
    selectThread: threadActorID =>
      dispatch(frameworkActions.selectThread(threadActorID)),
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
