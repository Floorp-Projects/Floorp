/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { DOM: dom, createFactory, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
  const { Headers } = createFactories(require("./headers"));
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));

  const { div } = dom;

  /**
   * This template represents the 'Headers' panel
   * s responsible for rendering its content.
   */
  let HeadersPanel = createClass({
    displayName: "HeadersPanel",

    propTypes: {
      actions: PropTypes.object,
      data: PropTypes.object,
    },

    getInitialState: function () {
      return {
        data: {}
      };
    },

    render: function () {
      let data = this.props.data;

      return (
        div({className: "headersPanelBox"},
          HeadersToolbar({actions: this.props.actions}),
          div({className: "panelContent"},
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
  let HeadersToolbar = createFactory(createClass({
    displayName: "HeadersToolbar",

    propTypes: {
      actions: PropTypes.object,
    },

    // Commands

    onCopy: function (event) {
      this.props.actions.onCopyHeaders();
    },

    render: function () {
      return (
        Toolbar({},
          ToolbarButton({className: "btn copy", onClick: this.onCopy},
            Locale.$STR("jsonViewer.Copy")
          )
        )
      );
    },
  }));

  // Exports from this module
  exports.HeadersPanel = HeadersPanel;
});
