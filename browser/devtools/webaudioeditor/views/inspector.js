/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/VariablesViewController.jsm");

// Strings for rendering
const EXPAND_INSPECTOR_STRING = L10N.getStr("expandInspector");
const COLLAPSE_INSPECTOR_STRING = L10N.getStr("collapseInspector");

// Store width as a preference rather than hardcode
// TODO bug 1009056
const INSPECTOR_WIDTH = 300;

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  searchEnabled: false,
  editableValueTooltip: "",
  editableNameTooltip: "",
  preventDisableOnChange: true,
  preventDescriptorModifiers: false,
  eval: () => {}
};

/**
 * Functions handling the audio node inspector UI.
 */

let InspectorView = {
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
  initialize: function () {
    this._tabsPane = $("#web-audio-editor-tabs");

    // Set up view controller
    this.el = $("#web-audio-inspector");
    this.el.setAttribute("width", INSPECTOR_WIDTH);
    this.button = $("#inspector-pane-toggle");
    mixin(this, ToggleMixin);
    this.bindToggle();

    // Hide inspector view on startup
    this.hideImmediately();

    this._onEval = this._onEval.bind(this);
    this._onNodeSelect = this._onNodeSelect.bind(this);
    this._onDestroyNode = this._onDestroyNode.bind(this);

    this._propsView = new VariablesView($("#properties-tabpanel-content"), GENERIC_VARIABLES_VIEW_SETTINGS);
    this._propsView.eval = this._onEval;

    window.on(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    gAudioNodes.on("remove", this._onDestroyNode);
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function () {
    this.unbindToggle();
    window.off(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    gAudioNodes.off("remove", this._onDestroyNode);

    this.el = null;
    this.button = null;
    this._tabsPane = null;
  },

  /**
   * Takes a AudioNodeView `node` and sets it as the current
   * node and scaffolds the inspector view based off of the new node.
   */
  setCurrentAudioNode: function (node) {
    this._currentNode = node || null;

    // If no node selected, set the inspector back to "no AudioNode selected"
    // view.
    if (!node) {
      $("#web-audio-editor-details-pane-empty").removeAttribute("hidden");
      $("#web-audio-editor-tabs").setAttribute("hidden", "true");
      window.emit(EVENTS.UI_INSPECTOR_NODE_SET, null);
    }
    // Otherwise load up the tabs view and hide the empty placeholder
    else {
      $("#web-audio-editor-details-pane-empty").setAttribute("hidden", "true");
      $("#web-audio-editor-tabs").removeAttribute("hidden");
      this._setTitle();
      this._buildPropertiesView()
        .then(() => window.emit(EVENTS.UI_INSPECTOR_NODE_SET, this._currentNode.id));
    }
  },

  /**
   * Returns the current AudioNodeView.
   */
  getCurrentAudioNode: function () {
    return this._currentNode;
  },

  /**
   * Empties out the props view.
   */
  resetUI: function () {
    this._propsView.empty();
    // Set current node to empty to load empty view
    this.setCurrentAudioNode();

    // Reset AudioNode inspector and hide
    this.hideImmediately();
  },

  /**
   * Sets the title of the Inspector view
   */
  _setTitle: function () {
    let node = this._currentNode;
    let title = node.type.replace(/Node$/, "");
    $("#web-audio-inspector-title").setAttribute("value", title);
  },

  /**
   * Reconstructs the `Properties` tab in the inspector
   * with the `this._currentNode` as it's source.
   */
  _buildPropertiesView: Task.async(function* () {
    let propsView = this._propsView;
    let node = this._currentNode;
    propsView.empty();

    let audioParamsScope = propsView.addScope("AudioParams");
    let props = yield node.getParams();

    // Disable AudioParams VariableView expansion
    // when there are no props i.e. AudioDestinationNode
    this._togglePropertiesView(!!props.length);

    props.forEach(({ param, value, flags }) => {
      let descriptor = {
        value: value,
        writable: !flags || !flags.readonly,
      };
      let item = audioParamsScope.addItem(param, descriptor);

      // No items should currently display a dropdown
      item.twisty = false;
    });

    audioParamsScope.expanded = true;

    window.emit(EVENTS.UI_PROPERTIES_TAB_RENDERED, node.id);
  }),

  _togglePropertiesView: function (show) {
    let propsView = $("#properties-tabpanel-content");
    let emptyView = $("#properties-tabpanel-content-empty");
    (show ? propsView : emptyView).removeAttribute("hidden");
    (show ? emptyView : propsView).setAttribute("hidden", "true");
  },

  /**
   * Returns the scope for AudioParams in the
   * VariablesView.
   *
   * @return Scope
   */
  _getAudioPropertiesScope: function () {
    return this._propsView.getScopeAtIndex(0);
  },

  /**
   * Event handlers
   */

  /**
   * Executed when an audio prop is changed in the UI.
   */
  _onEval: Task.async(function* (variable, value) {
    let ownerScope = variable.ownerView;
    let node = this._currentNode;
    let propName = variable.name;
    let error;

    if (!variable._initialDescriptor.writable) {
      error = new Error("Variable " + propName + " is not writable.");
    } else {
      // Cast value to proper type
      try {
        let number = parseFloat(value);
        if (!isNaN(number)) {
          value = number;
        } else {
          value = JSON.parse(value);
        }
        error = yield node.actor.setParam(propName, value);
      }
      catch (e) {
        error = e;
      }
    }

    // TODO figure out how to handle and display set prop errors
    // and enable `test/brorwser_wa_properties-view-edit.js`
    // Bug 994258
    if (!error) {
      ownerScope.get(propName).setGrip(value);
      window.emit(EVENTS.UI_SET_PARAM, node.id, propName, value);
    } else {
      window.emit(EVENTS.UI_SET_PARAM_ERROR, node.id, propName, value);
    }
  }),

  /**
   * Called on EVENTS.UI_SELECT_NODE, and takes an actorID `id`
   * and calls `setCurrentAudioNode` to scaffold the inspector view.
   */
  _onNodeSelect: function (_, id) {
    this.setCurrentAudioNode(gAudioNodes.get(id));

    // Ensure inspector is visible when selecting a new node
    this.show();
  },

  /**
   * Called when `DESTROY_NODE` is fired to remove the node from props view if
   * it's currently selected.
   */
  _onDestroyNode: function (node) {
    if (this._currentNode && this._currentNode.id === node.id) {
      this.setCurrentAudioNode(null);
    }
  }
};
