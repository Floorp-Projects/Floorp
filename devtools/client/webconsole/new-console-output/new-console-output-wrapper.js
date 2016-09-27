/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { configureStore } = require("devtools/client/webconsole/new-console-output/store");

const ConsoleOutput = React.createFactory(require("devtools/client/webconsole/new-console-output/components/console-output"));
const FilterBar = React.createFactory(require("devtools/client/webconsole/new-console-output/components/filter-bar"));

const store = configureStore();

function NewConsoleOutputWrapper(parentNode, jsterm, toolbox, owner) {
  const sourceMapService = toolbox ? toolbox._sourceMapService : null;
  let childComponent = ConsoleOutput({
    jsterm,
    sourceMapService,
    onViewSourceInDebugger: frame => toolbox.viewSourceInDebugger.call(
      toolbox,
      frame.url,
      frame.line
    ),
    openNetworkPanel: (requestId) => {
      return toolbox.selectTool("netmonitor").then(panel => {
        return panel.panelWin.NetMonitorController.inspectRequest(requestId);
      });
    },
    openLink: (url) => {
      owner.openLink(url);
    },
  });
  let filterBar = FilterBar({});
  let provider = React.createElement(
    Provider,
    { store },
    React.DOM.div(
      {className: "webconsole-output-wrapper"},
      filterBar,
      childComponent
  ));
  this.body = ReactDOM.render(provider, parentNode);
}

NewConsoleOutputWrapper.prototype = {
  dispatchMessageAdd: (message) => {
    store.dispatch(actions.messageAdd(message));
  },
  dispatchMessagesAdd: (messages) => {
    const batchedActions = messages.map(message => actions.messageAdd(message));
    store.dispatch(actions.batchActions(batchedActions));
  },
  dispatchMessagesClear: () => {
    store.dispatch(actions.messagesClear());
  },
};

// Exports from this module
module.exports = NewConsoleOutputWrapper;
