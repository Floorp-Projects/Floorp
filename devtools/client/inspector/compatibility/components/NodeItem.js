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

const {
  highlightNode,
  unhighlightNode,
} = require("devtools/client/inspector/boxmodel/actions/box-model-highlighter");

class NodeItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      node: Types.node.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { dispatch, node, setSelectedNode } = this.props;

    return dom.li(
      { className: "compatibility-node-item" },
      Rep({
        defaultRep: ElementNode,
        mode: MODE.TINY,
        object: translateNodeFrontToGrip(node),
        onDOMNodeClick: () => {
          setSelectedNode(node);
          dispatch(unhighlightNode());
        },
        onDOMNodeMouseOut: () => dispatch(unhighlightNode()),
        onDOMNodeMouseOver: () => dispatch(highlightNode(node)),
      })
    );
  }
}

module.exports = NodeItem;
