/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  translateNodeFrontToGrip,
} = require("resource://devtools/client/inspector/shared/utils.js");
const {
  REPS,
  MODE,
} = require("resource://devtools/client/shared/components/reps/index.js");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

const {
  highlightNode,
  unhighlightNode,
} = require("resource://devtools/client/inspector/boxmodel/actions/box-model-highlighter.js");

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
