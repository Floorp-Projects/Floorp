/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/Toolbar"));

  /**
   * This object represents a toolbar displayed within the
   * 'Raw Data' panel.
   */
  class TextToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
        isValidJson: PropTypes.bool,
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
      return Toolbar(
        {},
        ToolbarButton(
          {
            className: "btn save",
            onClick: this.onSave,
          },
          JSONView.Locale["jsonViewer.Save"]
        ),
        ToolbarButton(
          {
            className: "btn copy",
            onClick: this.onCopy,
          },
          JSONView.Locale["jsonViewer.Copy"]
        ),
        this.props.isValidJson
          ? ToolbarButton(
              {
                className: "btn prettyprint",
                onClick: this.onPrettify,
              },
              JSONView.Locale["jsonViewer.PrettyPrint"]
            )
          : null
      );
    }
  }

  // Exports from this module
  exports.TextToolbar = TextToolbar;
});
