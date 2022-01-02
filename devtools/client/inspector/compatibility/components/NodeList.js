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

const Types = require("devtools/client/inspector/compatibility/types");

const NodeItem = createFactory(
  require("devtools/client/inspector/compatibility/components/NodeItem")
);

class NodeList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      nodes: PropTypes.arrayOf(Types.node).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { dispatch, nodes, setSelectedNode } = this.props;

    return dom.ul(
      { className: "compatibility-node-list" },
      nodes.map(node =>
        NodeItem({
          dispatch,
          node,
          setSelectedNode,
        })
      )
    );
  }
}

module.exports = NodeList;
