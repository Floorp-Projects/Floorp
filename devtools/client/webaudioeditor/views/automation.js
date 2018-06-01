/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the audio node inspector UI.
 */

var AutomationView = {

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function() {
    this._buttons = $("#automation-param-toolbar-buttons");
    this.graph = new LineGraphWidget($("#automation-graph"), { avg: false });
    this.graph.selectionEnabled = false;

    this._onButtonClick = this._onButtonClick.bind(this);
    this._onNodeSet = this._onNodeSet.bind(this);
    this._onResize = this._onResize.bind(this);

    this._buttons.addEventListener("click", this._onButtonClick);
    window.on(EVENTS.UI_INSPECTOR_RESIZE, this._onResize);
    window.on(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSet);
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function() {
    this._buttons.removeEventListener("click", this._onButtonClick);
    window.off(EVENTS.UI_INSPECTOR_RESIZE, this._onResize);
    window.off(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSet);
  },

  /**
   * Empties out the props view.
   */
  resetUI: function() {
    this._currentNode = null;
  },

  /**
   * On a new node selection, create the Automation panel for
   * that specific node.
   */
  async build() {
    const node = this._currentNode;

    const props = await node.getParams();
    const params = props.filter(({ flags }) => flags && flags.param);

    this._createParamButtons(params);

    this._selectedParamName = params[0] ? params[0].param : null;
    this.render();
  },

  /**
   * Renders the graph for specified `paramName`. Called when
   * the parameter view is changed, or when new param data events
   * are fired for the currently specified param.
   */
  async render() {
    const node = this._currentNode;
    const paramName = this._selectedParamName;
    // Escape if either node or parameter name does not exist.
    if (!node || !paramName) {
      this._setState("no-params");
      window.emit(EVENTS.UI_AUTOMATION_TAB_RENDERED, null);
      return;
    }

    const { values, events } = await node.getAutomationData(paramName);
    this._setState(events.length ? "show" : "no-events");
    await this.graph.setDataWhenReady(values);
    window.emit(EVENTS.UI_AUTOMATION_TAB_RENDERED, node.id);
  },

  /**
   * Create the buttons for each AudioParam, that when clicked,
   * render the graph for that AudioParam.
   */
  _createParamButtons: function(params) {
    this._buttons.innerHTML = "";
    params.forEach((param, i) => {
      const button = document.createElement("toolbarbutton");
      button.setAttribute("class", "devtools-toolbarbutton automation-param-button");
      button.setAttribute("data-param", param.param);
      // Set label to the parameter name, should not be L10N'd
      button.setAttribute("label", param.param);

      // If first button, set to 'selected' for styling
      if (i === 0) {
        button.setAttribute("selected", true);
      }

      this._buttons.appendChild(button);
    });
  },

  /**
   * Internally sets the current audio node and rebuilds appropriate
   * views.
   */
  _setAudioNode: function(node) {
    this._currentNode = node;
    if (this._currentNode) {
      this.build();
    }
  },

  /**
   * Toggles the subviews to display messages whether or not
   * the audio node has no AudioParams, no automation events, or
   * shows the graph.
   */
  _setState: function(state) {
    const contentView = $("#automation-content");
    const emptyView = $("#automation-empty");

    const graphView = $("#automation-graph-container");
    const noEventsView = $("#automation-no-events");

    contentView.hidden = state === "no-params";
    emptyView.hidden = state !== "no-params";

    graphView.hidden = state !== "show";
    noEventsView.hidden = state !== "no-events";
  },

  /**
   * Event handlers
   */

  _onButtonClick: function(e) {
    Array.forEach($$(".automation-param-button"), $btn => $btn.removeAttribute("selected"));
    const paramName = e.target.getAttribute("data-param");
    e.target.setAttribute("selected", true);
    this._selectedParamName = paramName;
    this.render();
  },

  /**
   * Called when the inspector is resized.
   */
  _onResize: function() {
    this.graph.refresh();
  },

  /**
   * Called when the inspector view determines a node is selected.
   */
  _onNodeSet: function(id) {
    this._setAudioNode(id != null ? gAudioNodes.get(id) : null);
  }
};
