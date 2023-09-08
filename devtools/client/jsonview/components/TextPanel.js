/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { TextToolbar } = createFactories(
    require("devtools/client/jsonview/components/TextToolbar")
  );
  const { LiveText } = createFactories(
    require("devtools/client/jsonview/components/LiveText")
  );
  const { div } = dom;

  /**
   * This template represents the 'Raw Data' panel displaying
   * JSON as a text received from the server.
   */
  class TextPanel extends Component {
    static get propTypes() {
      return {
        isValidJson: PropTypes.bool,
        actions: PropTypes.object,
        errorMessage: PropTypes.string,
        data: PropTypes.instanceOf(Text),
      };
    }

    constructor(props) {
      super(props);
      this.state = {};
    }

    render() {
      return div(
        { className: "textPanelBox tab-panel-inner" },
        TextToolbar({
          actions: this.props.actions,
          isValidJson: this.props.isValidJson,
        }),
        this.props.errorMessage
          ? div({ className: "jsonParseError" }, this.props.errorMessage)
          : null,
        div({ className: "panelContent" }, LiveText({ data: this.props.data }))
      );
    }
  }

  // Exports from this module
  exports.TextPanel = TextPanel;
});
