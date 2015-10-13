/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource://devtools/client/shared/widgets/VariablesView.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesViewController.jsm");

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

var PropertiesView = {

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function () {
    this._onEval = this._onEval.bind(this);
    this._onNodeSet = this._onNodeSet.bind(this);

    window.on(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSet);
    this._propsView = new VariablesView($("#properties-content"), GENERIC_VARIABLES_VIEW_SETTINGS);
    this._propsView.eval = this._onEval;
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function () {
    window.off(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSet);
    this._propsView = null;
  },

  /**
   * Empties out the props view.
   */
  resetUI: function () {
    this._propsView.empty();
    this._currentNode = null;
  },

  /**
   * Internally sets the current audio node and rebuilds appropriate
   * views.
   */
  _setAudioNode: function (node) {
    this._currentNode = node;
    if (this._currentNode) {
      this._buildPropertiesView();
    }
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

  /**
   * Toggles the display of the "empty" properties view when
   * node has no properties to display.
   */
  _togglePropertiesView: function (show) {
    let propsView = $("#properties-content");
    let emptyView = $("#properties-empty");
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
   * Called when the inspector view determines a node is selected.
   */
  _onNodeSet: function (_, id) {
    this._setAudioNode(gAudioNodes.get(id));
  },

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
  })
};
