/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function LayoutViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;
  this.store = null;

  this.init();
}

LayoutViewTool.prototype = {

  init() {
    const { React, ReactDOM, ReactRedux, browserRequire } = this.inspector;

    const Store = browserRequire("devtools/client/inspector/layout/store");
    const App = React.createFactory(
      browserRequire("devtools/client/inspector/layout/components/App"));

    let store = this.store = Store();
    let provider = React.createElement(ReactRedux.Provider, { store }, App());
    ReactDOM.render(provider, this.document.querySelector("#layout-root"));
  },

  destroy() {
    this.inspector = null;
    this.document = null;
    this.store = null;
  },

};

exports.LayoutViewTool = LayoutViewTool;
