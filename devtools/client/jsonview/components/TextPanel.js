/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { TextToolbar } = createFactories(require("./TextToolbar"));

  const { div, pre } = dom;

  /**
   * This template represents the 'Raw Data' panel displaying
   * JSON as a text received from the server.
   */
  class TextPanel extends Component {
    static get propTypes() {
      return {
        isValidJson: PropTypes.bool,
        actions: PropTypes.object,
        data: PropTypes.string
      };
    }

    constructor(props) {
      super(props);
      this.state = {};
    }

    render() {
      return (
        div({className: "textPanelBox tab-panel-inner"},
          TextToolbar({
            actions: this.props.actions,
            isValidJson: this.props.isValidJson
          }),
          div({className: "panelContent"},
            pre({className: "data"},
              this.props.data
            )
          )
        )
      );
    }
  }

  // Exports from this module
  exports.TextPanel = TextPanel;
});
