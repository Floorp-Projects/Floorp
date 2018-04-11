/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* import-globals-from ../includes.js */

const MIN_INSPECTOR_WIDTH = 300;

// Strings for rendering
const EXPAND_INSPECTOR_STRING = L10N.getStr("expandInspector");
const COLLAPSE_INSPECTOR_STRING = L10N.getStr("collapseInspector");

/**
 * Functions handling the audio node inspector UI.
 */

var InspectorView = {
  _currentNode: null,

  // Set up config for view toggling
  _collapseString: COLLAPSE_INSPECTOR_STRING,
  _expandString: EXPAND_INSPECTOR_STRING,
  _toggleEvent: EVENTS.UI_INSPECTOR_TOGGLED,
  _animated: true,
  _delayed: true,

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function() {
    // Set up view controller
    this.el = $("#web-audio-inspector");
    this.splitter = $("#inspector-splitter");
    this.el.setAttribute("width", Services.prefs.getIntPref("devtools.webaudioeditor.inspectorWidth"));
    this.button = $("#inspector-pane-toggle");
    mixin(this, ToggleMixin);
    this.bindToggle();

    // Hide inspector view on startup
    this.hideImmediately();

    this._onNodeSelect = this._onNodeSelect.bind(this);
    this._onDestroyNode = this._onDestroyNode.bind(this);
    this._onResize = this._onResize.bind(this);
    this._onCommandClick = this._onCommandClick.bind(this);

    this.splitter.addEventListener("mouseup", this._onResize);
    for (let $el of $$("#audio-node-toolbar toolbarbutton")) {
      $el.addEventListener("command", this._onCommandClick);
    }
    window.on(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    gAudioNodes.on("remove", this._onDestroyNode);
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function() {
    this.unbindToggle();
    this.splitter.removeEventListener("mouseup", this._onResize);

    $("#audio-node-toolbar toolbarbutton").removeEventListener("command", this._onCommandClick);
    for (let $el of $$("#audio-node-toolbar toolbarbutton")) {
      $el.removeEventListener("command", this._onCommandClick);
    }
    window.off(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    gAudioNodes.off("remove", this._onDestroyNode);

    this.el = null;
    this.button = null;
    this.splitter = null;
  },

  /**
   * Takes a AudioNodeView `node` and sets it as the current
   * node and scaffolds the inspector view based off of the new node.
   */
  async setCurrentAudioNode(node) {
    this._currentNode = node || null;

    // If no node selected, set the inspector back to "no AudioNode selected"
    // view.
    if (!node) {
      $("#web-audio-editor-details-pane-empty").removeAttribute("hidden");
      $("#web-audio-editor-tabs").setAttribute("hidden", "true");
      window.emit(EVENTS.UI_INSPECTOR_NODE_SET, null);
    } else {
      // Otherwise load up the tabs view and hide the empty placeholder
      $("#web-audio-editor-details-pane-empty").setAttribute("hidden", "true");
      $("#web-audio-editor-tabs").removeAttribute("hidden");
      this._buildToolbar();
      window.emit(EVENTS.UI_INSPECTOR_NODE_SET, this._currentNode.id);
    }
  },

  /**
   * Returns the current AudioNodeView.
   */
  getCurrentAudioNode: function() {
    return this._currentNode;
  },

  /**
   * Empties out the props view.
   */
  resetUI: function() {
    // Set current node to empty to load empty view
    this.setCurrentAudioNode();

    // Reset AudioNode inspector and hide
    this.hideImmediately();
  },

  _buildToolbar: function() {
    let node = this.getCurrentAudioNode();

    let bypassable = node.bypassable;
    let bypassed = node.isBypassed();
    let button = $("#audio-node-toolbar .bypass");

    if (!bypassable) {
      button.setAttribute("disabled", true);
    } else {
      button.removeAttribute("disabled");
    }

    if (!bypassable || bypassed) {
      button.removeAttribute("checked");
    } else {
      button.setAttribute("checked", true);
    }
  },

  /**
   * Event handlers
   */

  /**
   * Called on EVENTS.UI_SELECT_NODE, and takes an actorID `id`
   * and calls `setCurrentAudioNode` to scaffold the inspector view.
   */
  _onNodeSelect: function(id) {
    this.setCurrentAudioNode(gAudioNodes.get(id));

    // Ensure inspector is visible when selecting a new node
    this.show();
  },

  _onResize: function() {
    if (this.el.getAttribute("width") < MIN_INSPECTOR_WIDTH) {
      this.el.setAttribute("width", MIN_INSPECTOR_WIDTH);
    }
    Services.prefs.setIntPref("devtools.webaudioeditor.inspectorWidth", this.el.getAttribute("width"));
    window.emit(EVENTS.UI_INSPECTOR_RESIZE);
  },

  /**
   * Called when `DESTROY_NODE` is fired to remove the node from props view if
   * it's currently selected.
   */
  _onDestroyNode: function(node) {
    if (this._currentNode && this._currentNode.id === node.id) {
      this.setCurrentAudioNode(null);
    }
  },

  _onCommandClick: function(e) {
    let node = this.getCurrentAudioNode();
    let button = e.target;
    let command = button.getAttribute("data-command");
    let checked = button.getAttribute("checked");

    if (button.getAttribute("disabled")) {
      return;
    }

    if (command === "bypass") {
      if (checked) {
        button.removeAttribute("checked");
        node.bypass(true);
      } else {
        button.setAttribute("checked", true);
        node.bypass(false);
      }
    }
  }
};
