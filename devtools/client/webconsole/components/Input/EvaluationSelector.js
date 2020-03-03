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

const actions = require("devtools/client/framework/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const threadSelectors = require("devtools/client/framework/reducers/threads");

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

class EvaluationSelector extends Component {
  static get propTypes() {
    return {
      selectThread: PropTypes.func.isRequired,
      selectedThread: PropTypes.object,
      threads: PropTypes.object,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  renderMenuItem(thread, i) {
    const { selectThread, selectedThread } = this.props;

    const label =
      thread.type == "mainThread"
        ? l10n.getStr("webconsole.input.selector.top")
        : thread.name;

    return MenuItem({
      key: `webconsole-evaluation-selector-item-${i}`,
      className: "menu-item webconsole-evaluation-selector-item",
      type: "checkbox",
      checked: selectedThread
        ? selectedThread == thread
        : thread.type == "mainThread",
      label,
      tooltip: thread.url,
      onClick: () => selectThread(thread),
    });
  }

  renderMenuItems() {
    const { threads } = this.props;

    let items = [];

    const mainThread = threads.find(thread => thread.type == "mainThread");
    const processes = threads.filter(thread => thread.type == "contentProcess");
    const workers = threads.filter(thread => thread.type == "worker");

    items.push(this.renderMenuItem(mainThread, 0));

    if (processes.length > 0) {
      items = [
        ...items,
        MenuItem({ role: "menuseparator" }),
        ...processes.map((thread, i) => this.renderMenuItem(thread, i + 1)),
      ];
    }

    if (workers.length > 0) {
      items = [
        ...items,
        MenuItem({ role: "menuseparator" }),
        ...workers.map((thread, i) =>
          this.renderMenuItem(thread, i + items.length)
        ),
      ];
    }

    return MenuList({ id: "webconsole-console-settings-menu-list" }, items);
  }

  getLabel() {
    const { selectedThread } = this.props;

    if (!selectedThread || selectedThread.type == "mainThread") {
      return l10n.getStr("webconsole.input.selector.top");
    }

    return selectedThread.name;
  }

  render() {
    const { webConsoleUI, threads } = this.props;
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
          "webconsole-evaluation-selector-button devtools-button devtools-dropdown-button",
        title: l10n.getStr("webconsole.input.selector.tooltip"),
      },
      // We pass the children in a function so we don't require the MenuItem and MenuList
      // components until we need to display them (i.e. when the button is clicked).
      () => this.renderMenuItems()
    );
  }
}

module.exports = connect(
  state => ({
    threads: threadSelectors.getToolboxThreads(state),
    selectedThread: threadSelectors.getSelectedThread(state),
  }),
  dispatch => ({
    selectThread: thread => dispatch(actions.selectThread(thread)),
  }),
  undefined,
  { storeKey: "toolbox-store" }
)(EvaluationSelector);
