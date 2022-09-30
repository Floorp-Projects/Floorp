/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("resource://devtools/client/shared/vendor/react.js");
const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  Provider,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

// DOM Panel
const MainFrame = React.createFactory(
  require("resource://devtools/client/dom/content/components/MainFrame.js")
);

// Store
const createStore = require("resource://devtools/client/shared/redux/create-store.js");

const {
  reducers,
} = require("resource://devtools/client/dom/content/reducers/index.js");
const store = createStore(reducers);

/**
 * This object represents view of the DOM panel and is responsible
 * for rendering the content. It renders the top level ReactJS
 * component: the MainFrame.
 */
function DomView(localStore) {
  addEventListener("devtools/chrome/message", this.onMessage.bind(this), true);

  // Make it local so, tests can access it.
  this.store = localStore;
}

DomView.prototype = {
  initialize(rootGrip) {
    const content = document.querySelector("#content");
    const mainFrame = MainFrame({
      object: rootGrip,
    });

    // Render top level component
    const provider = React.createElement(
      Provider,
      {
        store: this.store,
      },
      mainFrame
    );

    this.mainFrame = ReactDOM.render(provider, content);
  },

  onMessage(event) {
    const data = event.data;
    const method = data.type;

    if (typeof this[method] == "function") {
      this[method](data.args);
    }
  },
};

// Construct DOM panel view object and expose it to tests.
// Tests can access it through: |panel.panelWin.view|
window.view = new DomView(store);
