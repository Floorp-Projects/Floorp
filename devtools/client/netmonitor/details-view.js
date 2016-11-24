/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */
/* globals window, dumpn, $ */

"use strict";

const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const { Task } = require("devtools/shared/task");
const { ToolSidebar } = require("devtools/client/framework/sidebar");
const { EVENTS } = require("./events");
const { Filters } = require("./filter-predicates");
const { createFactory } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const CookiesPanel = createFactory(require("./shared/components/cookies-panel"));
const HeadersPanel = createFactory(require("./shared/components/headers-panel"));
const ParamsPanel = createFactory(require("./shared/components/params-panel"));
const PreviewPanel = createFactory(require("./shared/components/preview-panel"));
const ResponsePanel = createFactory(require("./shared/components/response-panel"));
const SecurityPanel = createFactory(require("./shared/components/security-panel"));
const TimingsPanel = createFactory(require("./shared/components/timings-panel"));

/**
 * Functions handling the requests details view.
 */
function DetailsView() {
  dumpn("DetailsView was instantiated");

  // The ToolSidebar requires the panel object to be able to emit events.
  EventEmitter.decorate(this);

  this._onTabSelect = this._onTabSelect.bind(this);
}

DetailsView.prototype = {
  /**
   * An object containing the state of tabs.
   */
  _viewState: {
    // if updating[tab] is true a task is currently updating the given tab.
    updating: [],
    // if dirty[tab] is true, the tab needs to be repopulated once current
    // update task finishes
    dirty: [],
    // the most recently received attachment data for the request
    latestData: null,
  },

  /**
   * Initialization function, called when the network monitor is started.
   */
  initialize: function (store) {
    dumpn("Initializing the DetailsView");

    this._cookiesPanelNode = $("#react-cookies-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      CookiesPanel()
    ), this._cookiesPanelNode);

    this._headersPanelNode = $("#react-headers-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      HeadersPanel()
    ), this._headersPanelNode);

    this._paramsPanelNode = $("#react-params-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      ParamsPanel()
    ), this._paramsPanelNode);

    this._previewPanelNode = $("#react-preview-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      PreviewPanel()
    ), this._previewPanelNode);

    this._responsePanelNode = $("#react-response-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      ResponsePanel()
    ), this._responsePanelNode);

    this._securityPanelNode = $("#react-security-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      SecurityPanel()
    ), this._securityPanelNode);

    this._timingsPanelNode = $("#react-timings-tabpanel-hook");

    ReactDOM.render(Provider(
      { store },
      TimingsPanel()
    ), this._timingsPanelNode);

    this.widget = $("#event-details-pane");
    this.sidebar = new ToolSidebar(this.widget, this, "netmonitor", {
      disableTelemetry: true,
      showAllTabsMenu: true
    });
    $("tabpanels", this.widget).addEventListener("select", this._onTabSelect);
  },

  /**
   * Destruction function, called when the network monitor is closed.
   */
  destroy: function () {
    dumpn("Destroying the DetailsView");
    ReactDOM.unmountComponentAtNode(this._cookiesPanelNode);
    ReactDOM.unmountComponentAtNode(this._headersPanelNode);
    ReactDOM.unmountComponentAtNode(this._paramsPanelNode);
    ReactDOM.unmountComponentAtNode(this._previewPanelNode);
    ReactDOM.unmountComponentAtNode(this._responsePanelNode);
    ReactDOM.unmountComponentAtNode(this._securityPanelNode);
    ReactDOM.unmountComponentAtNode(this._timingsPanelNode);
    this.sidebar.destroy();
    $("tabpanels", this.widget).removeEventListener("select",
      this._onTabSelect);
  },

  /**
   * Populates this view with the specified data.
   *
   * @param object data
   *        The data source (this should be the attachment of a request item).
   * @return object
   *        Returns a promise that resolves upon population the view.
   */
  populate: function (data) {
    let isHtml = Filters.html(data);

    // Show the "Preview" tabpanel only for plain HTML responses.
    this.sidebar.toggleTab(isHtml, "preview-tab");

    // Show the "Security" tab only for requests that
    //   1) are https (state != insecure)
    //   2) come from a target that provides security information.
    let hasSecurityInfo = data.securityState &&
                          data.securityState !== "insecure";
    this.sidebar.toggleTab(hasSecurityInfo, "security-tab");

    // Switch to the "Headers" tabpanel if the "Preview" previously selected
    // and this is not an HTML response or "Security" was selected but this
    // request has no security information.

    if (!isHtml && this.widget.selectedPanel === $("#preview-tabpanel") ||
        !hasSecurityInfo && this.widget.selectedPanel ===
          $("#security-tabpanel")) {
      this.widget.selectedIndex = 0;
    }

    this._dataSrc = { src: data, populated: [] };
    this._onTabSelect();
    window.emit(EVENTS.NETWORKDETAILSVIEW_POPULATED);

    return promise.resolve();
  },

  /**
   * Listener handling the tab selection event.
   */
  _onTabSelect: function () {
    let { src, populated } = this._dataSrc || {};
    let tab = this.widget.selectedIndex;
    let view = this;

    // Make sure the data source is valid and don't populate the same tab twice.
    if (!src || populated[tab]) {
      return;
    }

    let viewState = this._viewState;
    if (viewState.updating[tab]) {
      // A task is currently updating this tab. If we started another update
      // task now it would result in a duplicated content as described in bugs
      // 997065 and 984687. As there's no way to stop the current task mark the
      // tab dirty and refresh the panel once the current task finishes.
      viewState.dirty[tab] = true;
      viewState.latestData = src;
      return;
    }

    Task.spawn(function* () {
      viewState.updating[tab] = false;
    }).then(() => {
      if (tab == this.widget.selectedIndex) {
        if (viewState.dirty[tab]) {
          // The request information was updated while the task was running.
          viewState.dirty[tab] = false;
          view.populate(viewState.latestData);
        } else {
          // Tab is selected but not dirty. We're done here.
          populated[tab] = true;
          window.emit(EVENTS.TAB_UPDATED);
        }
      } else if (viewState.dirty[tab]) {
        // Tab is dirty but no longer selected. Don't refresh it now, it'll be
        // done if the tab is shown again.
        viewState.dirty[tab] = false;
      }
    }, e => console.error(e));
  },

  _dataSrc: null,
};

exports.DetailsView = DetailsView;
