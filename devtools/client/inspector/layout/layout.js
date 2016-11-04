/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } =
  require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./components/app"));
const Store = require("./store");

function LayoutView(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;
  this.store = null;

  this.init();
}

LayoutView.prototype = {

  init() {
    let store = this.store = Store();
    let provider = createElement(Provider, { store }, App());
    ReactDOM.render(provider, this.document.querySelector("#layoutview-container"));
  },

  destroy() {
    this.inspector = null;
    this.document = null;
    this.store = null;
  },
};

exports.LayoutView = LayoutView;

