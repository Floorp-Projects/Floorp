/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Types = require("devtools/client/inspector/compatibility/types");

const NodeList = createFactory(
  require("devtools/client/inspector/compatibility/components/NodeList")
);

class NodePane extends PureComponent {
  static get propTypes() {
    return {
      nodes: PropTypes.arrayOf(Types.node).isRequired,
      hideBoxModelHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelHighlighterForNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { nodes } = this.props;

    return nodes.length > 1
      ? dom.details(
          { className: "compatibility-node-pane" },
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
        )
      : dom.div({ className: "compatibility-node-pane" }, NodeList(this.props));
  }
}

module.exports = NodePane;
