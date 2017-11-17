/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { createFactory, Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  const { createFactories } = require("devtools/client/shared/react-utils");

  const { Headers } = createFactories(require("./Headers"));
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/Toolbar"));

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
        data: {}
      };
    }

    render() {
      let data = this.props.data;

      return (
        div({className: "headersPanelBox tab-panel-inner"},
          HeadersToolbarFactory({actions: this.props.actions}),
          div({className: "panelContent"},
            Headers({data: data})
          )
        )
      );
    }
  }

  /**
   * This template is responsible for rendering a toolbar
   * within the 'Headers' panel.
   */
  class HeadersToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.onCopy = this.onCopy.bind(this);
    }

    // Commands

    onCopy(event) {
      this.props.actions.onCopyHeaders();
    }

    render() {
      return (
        Toolbar({},
          ToolbarButton({className: "btn copy", onClick: this.onCopy},
            JSONView.Locale.$STR("jsonViewer.Copy")
          )
        )
      );
    }
  }

  let HeadersToolbarFactory = createFactory(HeadersToolbar);

  // Exports from this module
  exports.HeadersPanel = HeadersPanel;
});
