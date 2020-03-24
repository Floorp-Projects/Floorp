/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  const { createFactories } = require("devtools/client/shared/react-utils");
  const { div } = require("devtools/client/shared/vendor/react-dom-factories");

  const { SearchBox } = createFactories(
    require("devtools/client/jsonview/components/SearchBox")
  );
  const { Toolbar, ToolbarButton } = createFactories(
    require("devtools/client/jsonview/components/reps/Toolbar")
  );

  /* 100kB file */
  const EXPAND_THRESHOLD = 100 * 1024;

  /**
   * This template represents a toolbar within the 'JSON' panel.
   */
  class JsonToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
        dataSize: PropTypes.number,
      };
    }

    constructor(props) {
      super(props);
      this.onSave = this.onSave.bind(this);
      this.onCopy = this.onCopy.bind(this);
      this.onCollapse = this.onCollapse.bind(this);
      this.onExpand = this.onExpand.bind(this);
    }

    // Commands

    onSave(event) {
      this.props.actions.onSaveJson();
    }

    onCopy(event) {
      this.props.actions.onCopyJson();
    }

    onCollapse(event) {
      this.props.actions.onCollapse();
    }

    onExpand(event) {
      this.props.actions.onExpand();
    }

    render() {
      return Toolbar(
        {},
        ToolbarButton(
          { className: "btn save", onClick: this.onSave },
          JSONView.Locale["jsonViewer.Save"]
        ),
        ToolbarButton(
          { className: "btn copy", onClick: this.onCopy },
          JSONView.Locale["jsonViewer.Copy"]
        ),
        ToolbarButton(
          { className: "btn collapse", onClick: this.onCollapse },
          JSONView.Locale["jsonViewer.CollapseAll"]
        ),
        this.props.dataSize > EXPAND_THRESHOLD
          ? undefined
          : ToolbarButton(
              { className: "btn expand", onClick: this.onExpand },
              JSONView.Locale["jsonViewer.ExpandAll"]
            ),
        div({ className: "devtools-separator" }),
        SearchBox({
          actions: this.props.actions,
        })
      );
    }
  }

  // Exports from this module
  exports.JsonToolbar = JsonToolbar;
});
