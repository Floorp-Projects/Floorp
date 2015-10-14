/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

define(function(require, exports, module) {

const React = require("react");
const { createFactories } = require("./reps/rep-utils");
const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));
const DOM = React.DOM;

/**
 * This template represents the 'Raw Data' panel displaying
 * JSON as a text received from the server.
 */
var TextPanel = React.createClass({
  displayName: "TextPanel",

  getInitialState: function() {
    return {};
  },

  render: function() {
    return (
      DOM.div({className: "textPanelBox"},
        TextToolbar({actions: this.props.actions}),
        DOM.div({className: "panelContent"},
          DOM.pre({className: "data"},
            this.props.data
          )
        )
      )
    );
  }
});

/**
 * This object represents a toolbar displayed within the
 * 'Raw Data' panel.
 */
var TextToolbar = React.createFactory(React.createClass({
  displayName: "TextToolbar",

  render: function() {
    return (
      Toolbar({},
        ToolbarButton({className: "btn prettyprint",onClick: this.onPrettify},
          Locale.$STR("jsonViewer.PrettyPrint")
        ),
        ToolbarButton({className: "btn save", onClick: this.onSave},
          Locale.$STR("jsonViewer.Save")
        ),
        ToolbarButton({className: "btn copy", onClick: this.onCopy},
          Locale.$STR("jsonViewer.Copy")
        )
      )
    )
  },

  // Commands

  onPrettify: function(event) {
    this.props.actions.onPrettify();
  },

  onSave: function(event) {
    this.props.actions.onSaveJson();
  },

  onCopy: function(event) {
    this.props.actions.onCopyJson();
  },
}));

// Exports from this module
exports.TextPanel = TextPanel;
});
