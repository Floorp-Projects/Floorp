/* globals dumpn, $, NetMonitorView */
"use strict";

const { createFactory, DOM } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const FilterButtons = createFactory(require("./components/filter-buttons"));
const { L10N } = require("./l10n");

// Shortcuts
const { button } = DOM;

/**
 * Functions handling the toolbar view: expand/collapse button etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");

  this._onTogglePanesPressed = this._onTogglePanesPressed.bind(this);
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function (store) {
    dumpn("Initializing the ToolbarView");

    this._clearContainerNode = $("#react-clear-button-hook");
    this._filterContainerNode = $("#react-filter-buttons-hook");

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

    this._detailsPaneToggleButton = $("#details-pane-toggle");
    this._detailsPaneToggleButton.addEventListener("mousedown",
      this._onTogglePanesPressed, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function () {
    dumpn("Destroying the ToolbarView");

    ReactDOM.unmountComponentAtNode(this._clearContainerNode);
    ReactDOM.unmountComponentAtNode(this._filterContainerNode);

    this._detailsPaneToggleButton.removeEventListener("mousedown",
      this._onTogglePanesPressed, false);
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function () {
    let requestsMenu = NetMonitorView.RequestsMenu;
    let selectedIndex = requestsMenu.selectedIndex;

    // Make sure there's a selection if the button is pressed, to avoid
    // showing an empty network details pane.
    if (selectedIndex == -1 && requestsMenu.itemCount) {
      requestsMenu.selectedIndex = 0;
    } else {
      requestsMenu.selectedIndex = -1;
    }
  },

  _detailsPaneToggleButton: null
};

exports.ToolbarView = ToolbarView;
