/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
const require = window.windowRequire = BrowserLoader({
  baseURI: "resource://devtools/client/application/",
  window,
}).require;

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");

const App = createFactory(require("./src/components/App"));

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  bootstrap({ toolbox, panel }) {
    this.mount = document.querySelector("#mount");

    // Render the root Application component.
    const app = App();

    render(app, this.mount);
  },

  destroy() {
    unmountComponentAtNode(this.mount);
    this.mount = null;
  },
};
