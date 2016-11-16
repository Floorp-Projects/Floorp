/* globals dumpn, $, NetMonitorView */
"use strict";

const { createFactory, DOM } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const FilterButtons = createFactory(require("./components/filter-buttons"));
const ToggleButton = createFactory(require("./components/toggle-button"));
const SearchBox = createFactory(require("./components/search-box"));
const SummaryButton = createFactory(require("./components/summary-button"));
const { L10N } = require("./l10n");

// Shortcuts
const { button } = DOM;

/**
 * Functions handling the toolbar view: expand/collapse button etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function (store) {
    dumpn("Initializing the ToolbarView");

    this._clearContainerNode = $("#react-clear-button-hook");
    this._filterContainerNode = $("#react-filter-buttons-hook");
    this._summaryContainerNode = $("#react-summary-button-hook");
    this._searchContainerNode = $("#react-search-box-hook");
    this._toggleContainerNode = $("#react-details-pane-toggle-hook");

    // clear button
    ReactDOM.render(button({
      id: "requests-menu-clear-button",
      className: "devtools-button devtools-clear-icon",
      title: L10N.getStr("netmonitor.toolbar.clear"),
      onClick: () => {
        NetMonitorView.RequestsMenu.clear();
      }
    }), this._clearContainerNode);

    // filter button
    ReactDOM.render(Provider(
      { store },
      FilterButtons()
    ), this._filterContainerNode);

    // summary button
    ReactDOM.render(Provider(
      { store },
      SummaryButton()
    ), this._summaryContainerNode);

    // search box
    ReactDOM.render(Provider(
      { store },
      SearchBox()
    ), this._searchContainerNode);

    // details pane toggle button
    ReactDOM.render(Provider(
      { store },
      ToggleButton()
    ), this._toggleContainerNode);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function () {
    dumpn("Destroying the ToolbarView");

    ReactDOM.unmountComponentAtNode(this._clearContainerNode);
    ReactDOM.unmountComponentAtNode(this._filterContainerNode);
    ReactDOM.unmountComponentAtNode(this._summaryContainerNode);
    ReactDOM.unmountComponentAtNode(this._searchContainerNode);
    ReactDOM.unmountComponentAtNode(this._toggleContainerNode);
  }

};

exports.ToolbarView = ToolbarView;
