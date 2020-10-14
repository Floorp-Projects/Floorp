/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");
const { REPS, MODE } = require("devtools/client/shared/components/reps/index");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("devtools/client/inspector/compatibility/types");

class NodeItem extends PureComponent {
  static get propTypes() {
    return {
      node: Types.node.isRequired,
      hideBoxModelHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelHighlighterForNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      node,
      hideBoxModelHighlighter,
      setSelectedNode,
      showBoxModelHighlighterForNode,
    } = this.props;

    return dom.li(
      { className: "compatibility-node-item" },
      Rep({
        defaultRep: ElementNode,
        mode: MODE.TINY,
        object: translateNodeFrontToGrip(node),
        onDOMNodeClick: () => {
          setSelectedNode(node);
          hideBoxModelHighlighter();
        },
        onDOMNodeMouseOut: () => hideBoxModelHighlighter(),
        onDOMNodeMouseOver: () => showBoxModelHighlighterForNode(node),
      })
    );
  }
}

module.exports = NodeItem;
