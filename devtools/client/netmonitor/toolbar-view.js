/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals dumpn, $ */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const Toolbar = createFactory(require("./components/toolbar"));

/**
 * Functions handling the toolbar view: expand/collapse button etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize(store) {
    dumpn("Initializing the ToolbarView");

    this._toolbarNode = $("#react-toolbar-hook");

    ReactDOM.render(Provider(
      { store },
      Toolbar()
    ), this._toolbarNode);
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy() {
    dumpn("Destroying the ToolbarView");

    ReactDOM.unmountComponentAtNode(this._toolbarNode);
  }

};

exports.ToolbarView = ToolbarView;
