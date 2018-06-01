/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const App = createFactory(require("./components/App"));
const { EVENTS } = require("./constants");

const {
  getDisplayedRequestById,
} = require("./selectors/index");

/**
 * Global App object for Network panel. This object depends
 * on the UI and can't be created independently.
 *
 * This object can be consumed by other panels (e.g. Console
 * is using inspectRequest), by the Launchpad (bootstrap), etc.
 *
 * @param {Object} api An existing API object to be reused.
 */
function NetMonitorApp(api) {
  this.api = api;
}

NetMonitorApp.prototype = {
  async bootstrap({ toolbox, document }) {
    // Get the root element for mounting.
    this.mount = document.querySelector("#mount");

    const openLink = (link) => {
      const parentDoc = toolbox.doc;
      const iframe = parentDoc.getElementById("toolbox-panel-iframe-netmonitor");
      const top = iframe.ownerDocument.defaultView.top;
      top.openUILinkIn(link, "tab");
    };

    const openSplitConsole = (err) => {
      toolbox.openSplitConsole().then(() => {
        toolbox.target.logErrorInPage(err, "har");
      });
    };

    const {
      actions,
      connector,
      store,
    } = this.api;

    const sourceMapService = toolbox.sourceMapURLService;
    const app = App({
      actions,
      connector,
      openLink,
      openSplitConsole,
      sourceMapService
    });

    // Render the root Application component.
    render(Provider({ store: store }, app), this.mount);
  },

  /**
   * Clean up (unmount from DOM, remove listeners, disconnect).
   */
  async destroy() {
    unmountComponentAtNode(this.mount);

    // Make sure to destroy the API object. It's usually destroyed
    // in the Toolbox destroy method, but we need it here for case
    // where the Network panel is initialized without the toolbox
    // and running in a tab (see initialize.js for details).
    await this.api.destroy();
  },

  /**
   * Selects the specified request in the waterfall and opens the details view.
   * This is a firefox toolbox specific API, which providing an ability to inspect
   * a network request directly from other internal toolbox panel.
   *
   * @param {string} requestId The actor ID of the request to inspect.
   * @return {object} A promise resolved once the task finishes.
   */
  async inspectRequest(requestId) {
    const {
      actions,
      store,
    } = this.api;

    // Look for the request in the existing ones or wait for it to appear,
    // if the network monitor is still loading.
    return new Promise((resolve) => {
      let request = null;
      const inspector = () => {
        request = getDisplayedRequestById(store.getState(), requestId);
        if (!request) {
          // Reset filters so that the request is visible.
          actions.toggleRequestFilterType("all");
          request = getDisplayedRequestById(store.getState(), requestId);
        }

        // If the request was found, select it. Otherwise this function will be
        // called again once new requests arrive.
        if (request) {
          this.api.off(EVENTS.REQUEST_ADDED, inspector);
          actions.selectRequest(request.id);
          resolve();
        }
      };

      inspector();

      if (!request) {
        this.api.on(EVENTS.REQUEST_ADDED, inspector);
      }
    });
  }
};

exports.NetMonitorApp = NetMonitorApp;
