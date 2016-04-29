/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { combineReducers } = require("devtools/client/shared/vendor/redux");

const createStore = require("devtools/client/shared/redux/create-store")();

const { reducers } = require("./reducers/index");
const store = createStore(combineReducers(reducers));

const DummyChildComponent = React.createFactory(require("./dummy-child-component"));

function OutputWrapperThingy(parentNode) {
  let childComponent = DummyChildComponent({});
  let provider = React.createElement(Provider, { store: store }, childComponent);
  this.body = ReactDOM.render(provider, parentNode);
}

OutputWrapperThingy.prototype = {
  dispatchMessageAdd: (message) => {
    let action = {
      type: "MESSAGE_ADD",
      message,
    }
    store.dispatch(action)
  },
  dispatchMessagesClear: () => {
    let action = {
      type: "MESSAGES_CLEAR",
    }
    store.dispatch(action)
  }
};

// Exports from this module
module.exports = OutputWrapperThingy;
