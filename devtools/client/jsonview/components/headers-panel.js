/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

define(function(require, exports, module) {

const React = require("react");
const { createFactories } = require("./reps/rep-utils");
const { Headers } = createFactories(require("./headers"));
const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));

const DOM = React.DOM;

/**
 * This template represents the 'Headers' panel
 * s responsible for rendering its content.
 */
var HeadersPanel = React.createClass({
  displayName: "HeadersPanel",

  getInitialState: function() {
    return {
      data: {}
    };
  },

  render: function() {
    var data = this.props.data;

    return (
      DOM.div({className: "headersPanelBox"},
        HeadersToolbar({actions: this.props.actions}),
        DOM.div({className: "panelContent"},
          Headers({data: data})
        )
      )
    );
  }
});

/**
 * This template is responsible for rendering a toolbar
 * within the 'Headers' panel.
 */
var HeadersToolbar = React.createFactory(React.createClass({
  displayName: "HeadersToolbar",

  render: function() {
    return (
      Toolbar({},
        ToolbarButton({className: "btn copy", onClick: this.onCopy},
          Locale.$STR("jsonViewer.Copy")
        )
      )
    )
  },

  // Commands

  onCopy: function(event) {
    this.props.actions.onCopyHeaders();
  },
}));

// Exports from this module
exports.HeadersPanel = HeadersPanel;
});
