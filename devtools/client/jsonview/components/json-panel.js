/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { DOM: dom, createFactory, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
  const TreeView = createFactory(require("devtools/client/shared/components/tree/tree-view"));
  const { Rep } = createFactories(require("devtools/client/shared/components/reps/rep"));
  const { MODE } = require("devtools/client/shared/components/reps/constants");
  const { SearchBox } = createFactories(require("./search-box"));
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/toolbar"));

  const { div } = dom;
  const AUTO_EXPAND_MAX_SIZE = 100 * 1024;
  const AUTO_EXPAND_MAX_LEVEL = 7;

  /**
   * This template represents the 'JSON' panel. The panel is
   * responsible for rendering an expandable tree that allows simple
   * inspection of JSON structure.
   */
  let JsonPanel = createClass({
    displayName: "JsonPanel",

    propTypes: {
      data: PropTypes.oneOfType([
        PropTypes.string,
        PropTypes.array,
        PropTypes.object
      ]),
      jsonTextLength: PropTypes.number,
      searchFilter: PropTypes.string,
      actions: PropTypes.object,
    },

    getInitialState: function () {
      return {};
    },

    componentDidMount: function () {
      document.addEventListener("keypress", this.onKeyPress, true);
    },

    componentWillUnmount: function () {
      document.removeEventListener("keypress", this.onKeyPress, true);
    },

    onKeyPress: function (e) {
      // XXX shortcut for focusing the Filter field (see Bug 1178771).
    },

    onFilter: function (object) {
      if (!this.props.searchFilter) {
        return true;
      }

      let json = object.name + JSON.stringify(object.value);
      return json.toLowerCase().indexOf(this.props.searchFilter.toLowerCase()) >= 0;
    },

    getExpandedNodes: function (object, path = "", level = 0) {
      if (typeof object != "object") {
        return null;
      }

      if (level > AUTO_EXPAND_MAX_LEVEL) {
        return null;
      }

      let expandedNodes = new Set();
      for (let prop in object) {
        let nodePath = path + "/" + prop;
        expandedNodes.add(nodePath);

        let nodes = this.getExpandedNodes(object[prop], nodePath, level + 1);
        if (nodes) {
          expandedNodes = new Set([...expandedNodes, ...nodes]);
        }
      }
      return expandedNodes;
    },

    renderValue: props => {
      let member = props.member;

      // Hide object summary when object is expanded (bug 1244912).
      if (typeof member.value == "object" && member.open) {
        return null;
      }

      // Render the value (summary) using Reps library.
      return Rep(Object.assign({}, props, {
        cropLimit: 50,
      }));
    },

    renderTree: function () {
      // Append custom column for displaying values. This column
      // Take all available horizontal space.
      let columns = [{
        id: "value",
        width: "100%"
      }];

      // Expand the document by default if its size isn't bigger than 100KB.
      let expandedNodes = new Set();
      if (this.props.jsonTextLength <= AUTO_EXPAND_MAX_SIZE) {
        expandedNodes = this.getExpandedNodes(this.props.data);
      }

      // Render tree component.
      return TreeView({
        object: this.props.data,
        mode: MODE.TINY,
        onFilter: this.onFilter,
        columns: columns,
        renderValue: this.renderValue,
        expandedNodes: expandedNodes,
      });
    },

    render: function () {
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
    displayName: "JsonToolbar",

    propTypes: {
      actions: PropTypes.object,
    },

    // Commands

    onSave: function (event) {
      this.props.actions.onSaveJson();
    },

    onCopy: function (event) {
      this.props.actions.onCopyJson();
    },

    render: function () {
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
