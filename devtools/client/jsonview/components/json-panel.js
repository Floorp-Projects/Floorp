/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { DOM: dom, createFactory, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
  const TreeView = createFactory(require("devtools/client/shared/components/tree/tree-view"));
  const { Rep } = createFactories(require("devtools/client/shared/components/reps/rep"));
  const { SearchBox } = createFactories(require("./search-box"));
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));

  const { div } = dom;

  /**
   * This template represents the 'JSON' panel. The panel is
   * responsible for rendering an expandable tree that allows simple
   * inspection of JSON structure.
   */
  let JsonPanel = createClass({
    propTypes: {
      data: PropTypes.oneOfType([
        PropTypes.string,
        PropTypes.array,
        PropTypes.object
      ]),
      searchFilter: PropTypes.string,
      actions: PropTypes.object,
    },

    displayName: "JsonPanel",

    getInitialState: function() {
      return {};
    },

    componentDidMount: function() {
      document.addEventListener("keypress", this.onKeyPress, true);
    },

    componentWillUnmount: function() {
      document.removeEventListener("keypress", this.onKeyPress, true);
    },

    onKeyPress: function(e) {
      // XXX shortcut for focusing the Filter field (see Bug 1178771).
    },

    onFilter: function(object) {
      if (!this.props.searchFilter) {
        return true;
      }

      let json = JSON.stringify(object).toLowerCase();
      return json.indexOf(this.props.searchFilter) >= 0;
    },

    renderValue: props => {
      let member = props.member;

      // Hide object summary when object is expanded (bug 1244912).
      if (typeof member.value == "object" && member.open) {
        return null;
      }

      // Render the value (summary) using Reps library.
      return Rep(props);
    },

    renderTree: function() {
      // Append custom column for displaying values. This column
      // Take all available horizontal space.
      let columns = [{
        id: "value",
        width: "100%"
      }];

      // Render tree component.
      return TreeView({
        object: this.props.data,
        mode: "tiny",
        onFilter: this.onFilter.bind(this),
        columns: columns,
        renderValue: this.renderValue
      });
    },

    render: function() {
      let content;
      let data = this.props.data;

      try {
        if (typeof data == "object") {
          content = this.renderTree();
        } else {
          content = div({className: "jsonParseError"},
            data + ""
          );
        }
      } catch (err) {
        content = div({className: "jsonParseError"},
          err + ""
        );
      }

      return (
        div({className: "jsonPanelBox"},
          JsonToolbar({actions: this.props.actions}),
          div({className: "panelContent"},
            content
          )
        )
      );
    }
  });

  /**
   * This template represents a toolbar within the 'JSON' panel.
   */
  let JsonToolbar = createFactory(createClass({
    propTypes: {
      actions: PropTypes.object,
    },

    displayName: "JsonToolbar",

    // Commands

    onSave: function(event) {
      this.props.actions.onSaveJson();
    },

    onCopy: function(event) {
      this.props.actions.onCopyJson();
    },

    render: function() {
      return (
        Toolbar({},
          ToolbarButton({className: "btn save", onClick: this.onSave},
            Locale.$STR("jsonViewer.Save")
          ),
          ToolbarButton({className: "btn copy", onClick: this.onCopy},
            Locale.$STR("jsonViewer.Copy")
          ),
          SearchBox({
            actions: this.props.actions
          })
        )
      );
    },
  }));

  // Exports from this module
  exports.JsonPanel = JsonPanel;
});
