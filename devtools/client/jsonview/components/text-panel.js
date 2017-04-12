/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { DOM: dom, createFactory, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

  const { createFactories } = require("devtools/client/shared/react-utils");
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));
  const { div, pre } = dom;

  /**
   * This template represents the 'Raw Data' panel displaying
   * JSON as a text received from the server.
   */
  let TextPanel = createClass({
    displayName: "TextPanel",

    propTypes: {
      actions: PropTypes.object,
      data: PropTypes.string
    },

    getInitialState: function () {
      return {};
    },

    render: function () {
      return (
        div({className: "textPanelBox tab-panel-inner"},
          TextToolbar({actions: this.props.actions}),
          div({className: "panelContent"},
            pre({className: "data"},
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
  let TextToolbar = createFactory(createClass({
    displayName: "TextToolbar",

    propTypes: {
      actions: PropTypes.object,
    },

    // Commands

    onPrettify: function (event) {
      this.props.actions.onPrettify();
    },

    onSave: function (event) {
      this.props.actions.onSaveJson();
    },

    onCopy: function (event) {
      this.props.actions.onCopyJson();
    },

    render: function () {
      return (
        Toolbar({},
          ToolbarButton({
            className: "btn save",
            onClick: this.onSave},
            Locale.$STR("jsonViewer.Save")
          ),
          ToolbarButton({
            className: "btn copy",
            onClick: this.onCopy},
            Locale.$STR("jsonViewer.Copy")
          ),
          ToolbarButton({
            className: "btn prettyprint",
            onClick: this.onPrettify},
            Locale.$STR("jsonViewer.PrettyPrint")
          )
        )
      );
    },
  }));

  // Exports from this module
  exports.TextPanel = TextPanel;
});
