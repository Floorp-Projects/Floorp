/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { createFactory, Component } = require("devtools/client/shared/vendor/react");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const TreeView =
    createFactory(require("devtools/client/shared/components/tree/TreeView"));

  const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { Rep } = REPS;

  const { SearchBox } = createFactories(require("./SearchBox"));
  const { Toolbar, ToolbarButton } = createFactories(require("./reps/Toolbar"));

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

      let json = object.name + JSON.stringify(object.value);
      return json.toLowerCase().indexOf(this.props.searchFilter.toLowerCase()) >= 0;
    }

    renderValue(props) {
      let member = props.member;

      // Hide object summary when non-empty object is expanded (bug 1244912).
      if (isObject(member.value) && member.hasChildren && member.open) {
        return null;
      }

      // Render the value (summary) using Reps library.
      return Rep(Object.assign({}, props, {
        cropLimit: 50,
        noGrip: true,
        omitLinkHref: false,
      }));
    }

    renderTree() {
      // Append custom column for displaying values. This column
      // Take all available horizontal space.
      let columns = [{
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
      let data = this.props.data;

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
          JsonToolbarFactory({actions: this.props.actions}),
          div({className: "panelContent"},
            content
          )
        )
      );
    }
  }

  /**
   * This template represents a toolbar within the 'JSON' panel.
   */
  class JsonToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.onSave = this.onSave.bind(this);
      this.onCopy = this.onCopy.bind(this);
    }

    // Commands

    onSave(event) {
      this.props.actions.onSaveJson();
    }

    onCopy(event) {
      this.props.actions.onCopyJson();
    }

    render() {
      return (
        Toolbar({},
          ToolbarButton({className: "btn save", onClick: this.onSave},
            JSONView.Locale.$STR("jsonViewer.Save")
          ),
          ToolbarButton({className: "btn copy", onClick: this.onCopy},
            JSONView.Locale.$STR("jsonViewer.Copy")
          ),
          SearchBox({
            actions: this.props.actions
          })
        )
      );
    }
  }

  let JsonToolbarFactory = createFactory(JsonToolbar);

  // Exports from this module
  exports.JsonPanel = JsonPanel;
});
