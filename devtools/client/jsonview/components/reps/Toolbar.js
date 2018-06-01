/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  /**
   * Renders a simple toolbar.
   */
  class Toolbar extends Component {
    static get propTypes() {
      return {
        children: PropTypes.oneOfType([
          PropTypes.array,
          PropTypes.element
        ])
      };
    }

    render() {
      return (
        dom.div({className: "toolbar"},
          this.props.children
        )
      );
    }
  }

  /**
   * Renders a simple toolbar button.
   */
  class ToolbarButton extends Component {
    static get propTypes() {
      return {
        active: PropTypes.bool,
        disabled: PropTypes.bool,
        children: PropTypes.string,
      };
    }

    render() {
      const props = Object.assign({className: "btn"}, this.props);
      return (
        dom.button(props, this.props.children)
      );
    }
  }

  // Exports from this module
  exports.Toolbar = Toolbar;
  exports.ToolbarButton = ToolbarButton;
});
