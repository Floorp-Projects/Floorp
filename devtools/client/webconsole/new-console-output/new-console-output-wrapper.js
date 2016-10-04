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
let queuedActions = [];
let throttledDispatchTimeout = false;

function NewConsoleOutputWrapper(parentNode, jsterm, toolbox, owner) {
  this.parentNode = parentNode;
  this.jsterm = jsterm;
  this.toolbox = toolbox;
  this.owner = owner;

  this.init = this.init.bind(this);
}

NewConsoleOutputWrapper.prototype = {
  init: function () {
    const sourceMapService = this.toolbox ? this.toolbox._sourceMapService : null;

    let childComponent = ConsoleOutput({
      hudProxyClient: this.jsterm.hud.proxy.client,
      sourceMapService,
      onViewSourceInDebugger: frame => this.toolbox.viewSourceInDebugger.call(
        this.toolbox,
        frame.url,
        frame.line
      ),
      openNetworkPanel: (requestId) => {
        return this.toolbox.selectTool("netmonitor").then(panel => {
          return panel.panelWin.NetMonitorController.inspectRequest(requestId);
        });
      },
      openLink: (url) => {
        this.owner.openLink(url);
      },
      emitNewMessage: (node) => {
        this.jsterm.hud.emit("new-messages", new Set([{
          node
        }]));
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

    this.body = ReactDOM.render(provider, this.parentNode);
  },
  dispatchMessageAdd: (message) => {
    batchedMessageAdd(actions.messageAdd(message));
  },
  dispatchMessagesAdd: (messages) => {
    const batchedActions = messages.map(message => actions.messageAdd(message));
    store.dispatch(actions.batchActions(batchedActions));
  },
  dispatchMessagesClear: () => {
    store.dispatch(actions.messagesClear());
  },
};

function batchedMessageAdd(action) {
  queuedActions.push(action);
  if (!throttledDispatchTimeout) {
    throttledDispatchTimeout = setTimeout(() => {
      store.dispatch(actions.batchActions(queuedActions));
      queuedActions = [];
      throttledDispatchTimeout = null;
    }, 50);
  }
}

// Exports from this module
module.exports = NewConsoleOutputWrapper;
