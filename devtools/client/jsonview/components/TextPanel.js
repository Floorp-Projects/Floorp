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
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/Toolbar"));
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
          TextToolbarFactory({
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

  /**
   * This object represents a toolbar displayed within the
   * 'Raw Data' panel.
   */
  class TextToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
        isValidJson: PropTypes.bool
      };
    }

    constructor(props) {
      super(props);
      this.onPrettify = this.onPrettify.bind(this);
      this.onSave = this.onSave.bind(this);
      this.onCopy = this.onCopy.bind(this);
    }

    // Commands

    onPrettify(event) {
      this.props.actions.onPrettify();
    }

    onSave(event) {
      this.props.actions.onSaveJson();
    }

    onCopy(event) {
      this.props.actions.onCopyJson();
    }

    render() {
      return (
        Toolbar({},
          ToolbarButton({
            className: "btn save",
            onClick: this.onSave},
            JSONView.Locale.$STR("jsonViewer.Save")
          ),
          ToolbarButton({
            className: "btn copy",
            onClick: this.onCopy},
            JSONView.Locale.$STR("jsonViewer.Copy")
          ),
          this.props.isValidJson ?
            ToolbarButton({
              className: "btn prettyprint",
              onClick: this.onPrettify},
              JSONView.Locale.$STR("jsonViewer.PrettyPrint")
            ) :
            null
        )
      );
    }
  }

  let TextToolbarFactory = createFactory(TextToolbar);

  // Exports from this module
  exports.TextPanel = TextPanel;
});
