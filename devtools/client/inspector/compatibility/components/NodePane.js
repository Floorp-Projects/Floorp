/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

const NodeList = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/NodeList.js")
);

class NodePane extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      nodes: PropTypes.arrayOf(Types.node).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { nodes } = this.props;

    return dom.details(
      {
        className: "compatibility-node-pane",
        open: nodes.length <= 1,
      },
      Localized(
        {
          id: "compatibility-issue-occurrences",
          $number: nodes.length,
        },
        dom.summary(
          { className: "compatibility-node-pane__summary" },
          "compatibility-issue-occurrences"
        )
      ),
      NodeList(this.props)
    );
  }
}

module.exports = NodePane;
