/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const { configureStore } = require("devtools/client/webconsole/new-console-output/store");

const ConsoleOutput = React.createFactory(require("devtools/client/webconsole/new-console-output/components/console-output"));
const FilterBar = React.createFactory(require("devtools/client/webconsole/new-console-output/components/filter-bar"));

const store = configureStore();

function NewConsoleOutputWrapper(parentNode, jsterm, toolbox) {
  let childComponent = ConsoleOutput({
    jsterm,
    onViewSourceInDebugger: frame => toolbox.viewSourceInDebugger.call(
      toolbox,
      frame.url,
      frame.line
    )
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
  dispatchMessagesClear: () => {
    store.dispatch(actions.messagesClear());
  }
};

// Exports from this module
module.exports = NewConsoleOutputWrapper;
