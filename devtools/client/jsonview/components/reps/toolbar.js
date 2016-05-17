/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");
  const DOM = React.DOM;

  /**
   * Renders a simple toolbar.
   */
  let Toolbar = React.createClass({
    displayName: "Toolbar",

    propTypes: {
      children: React.PropTypes.oneOfType([
        React.PropTypes.array,
        React.PropTypes.element
      ])
    },

    render: function () {
      return (
        DOM.div({className: "toolbar"},
          this.props.children
        )
      );
    }
  });

  /**
   * Renders a simple toolbar button.
   */
  let ToolbarButton = React.createClass({
    displayName: "ToolbarButton",

    propTypes: {
      active: React.PropTypes.bool,
      disabled: React.PropTypes.bool,
      children: React.PropTypes.string,
    },

    render: function () {
      let props = Object.assign({className: "btn"}, this.props);
      return (
        DOM.button(props, this.props.children)
      );
    },
  });

  // Exports from this module
  exports.Toolbar = Toolbar;
  exports.ToolbarButton = ToolbarButton;
});
