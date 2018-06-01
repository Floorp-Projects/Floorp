/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { createFactory, Component } = require("devtools/client/shared/vendor/react");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { createFactories } = require("devtools/client/shared/react-utils");

  const TreeView =
    createFactory(require("devtools/client/shared/components/tree/TreeView"));
  const { JsonToolbar } = createFactories(require("./JsonToolbar"));

  const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
  const { Rep } = REPS;

  const { div } = dom;

  function isObject(value) {
    return Object(value) === value;
  }

  /**
   * This template represents the 'JSON' panel. The panel is
   * responsible for rendering an expandable tree that allows simple
   * inspection of JSON structure.
   */
  class JsonPanel extends Component {
    static get propTypes() {
      return {
        data: PropTypes.oneOfType([
          PropTypes.string,
          PropTypes.array,
          PropTypes.object,
          PropTypes.bool,
          PropTypes.number
        ]),
        dataSize: PropTypes.number,
        expandedNodes: PropTypes.instanceOf(Set),
        searchFilter: PropTypes.string,
        actions: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.state = {};
      this.onKeyPress = this.onKeyPress.bind(this);
      this.onFilter = this.onFilter.bind(this);
      this.renderValue = this.renderValue.bind(this);
      this.renderTree = this.renderTree.bind(this);
    }

    componentDidMount() {
      document.addEventListener("keypress", this.onKeyPress, true);
    }

    componentWillUnmount() {
      document.removeEventListener("keypress", this.onKeyPress, true);
    }

    onKeyPress(e) {
      // XXX shortcut for focusing the Filter field (see Bug 1178771).
    }

    onFilter(object) {
      if (!this.props.searchFilter) {
        return true;
      }

      const json = object.name + JSON.stringify(object.value);
      return json.toLowerCase().includes(this.props.searchFilter.toLowerCase());
    }

    renderValue(props) {
      const member = props.member;

      // Hide object summary when non-empty object is expanded (bug 1244912).
      if (isObject(member.value) && member.hasChildren && member.open) {
        return null;
      }

      // Render the value (summary) using Reps library.
      return Rep(Object.assign({}, props, {
        cropLimit: 50,
        noGrip: true,
        openLink(str) {
          try {
            const u = new URL(str);
            if (u.protocol == "https:" || u.protocol == "http:") {
              window.open(str, "_blank");
            }
          } catch (ex) { /* the link might be bust, then we do nothing */ }
        },
      }));
    }

    renderTree() {
      // Append custom column for displaying values. This column
      // Take all available horizontal space.
      const columns = [{
        id: "value",
        width: "100%"
      }];

      // Render tree component.
      return TreeView({
        object: this.props.data,
        mode: MODE.TINY,
        onFilter: this.onFilter,
        columns: columns,
        renderValue: this.renderValue,
        expandedNodes: this.props.expandedNodes,
      });
    }

    render() {
      let content;
      const data = this.props.data;

      if (!isObject(data)) {
        content = div({className: "jsonPrimitiveValue"}, Rep({
          object: data
        }));
      } else if (data instanceof Error) {
        content = div({className: "jsonParseError"},
          data + ""
        );
      } else {
        content = this.renderTree();
      }

      return (
        div({className: "jsonPanelBox tab-panel-inner"},
          JsonToolbar({actions: this.props.actions, dataSize: this.props.dataSize}),
          div({className: "panelContent"},
            content
          )
        )
      );
    }
  }

  // Exports from this module
  exports.JsonPanel = JsonPanel;
});
