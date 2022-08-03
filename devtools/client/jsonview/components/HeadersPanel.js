/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  const { createFactories } = require("devtools/client/shared/react-utils");

  const { Headers } = createFactories(
    require("devtools/client/jsonview/components/Headers")
  );
  const { HeadersToolbar } = createFactories(
    require("devtools/client/jsonview/components/HeadersToolbar")
  );

  const { div } = dom;

  /**
   * This template represents the 'Headers' panel
   * s responsible for rendering its content.
   */
  class HeadersPanel extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
        data: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        data: {},
      };
    }

    render() {
      const data = this.props.data;

      return div(
        { className: "headersPanelBox tab-panel-inner" },
        HeadersToolbar({ actions: this.props.actions }),
        div({ className: "panelContent" }, Headers({ data }))
      );
    }
  }

  // Exports from this module
  exports.HeadersPanel = HeadersPanel;
});
