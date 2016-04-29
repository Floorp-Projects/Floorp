/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const DummyChildComponent = React.createFactory(require("./dummy-child-component"));

function OutputWrapperThingy(parentNode, store) {
  let childComponent = DummyChildComponent({});
  let provider = React.createElement(Provider, { store: store }, childComponent);
  this.body = ReactDOM.render(provider, parentNode);
}

// Exports from this module
module.exports = OutputWrapperThingy;
