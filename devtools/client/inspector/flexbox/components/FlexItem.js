/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

loader.lazyRequireGetter(
  this,
  "getNodeRep",
  "devtools/client/inspector/shared/node-reps"
);

const Types = require("devtools/client/inspector/flexbox/types");

const {
  highlightNode,
  unhighlightNode,
} = require("devtools/client/inspector/boxmodel/actions/box-model-highlighter");

class FlexItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
      index: PropTypes.number.isRequired,
      scrollToTop: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      dispatch,
      flexItem,
      index,
      scrollToTop,
      setSelectedNode,
    } = this.props;
    const { nodeFront } = flexItem;

    return dom.button(
      {
        className: "devtools-button devtools-monospace",
        onClick: e => {
          e.stopPropagation();
          scrollToTop();
          setSelectedNode(nodeFront);
          dispatch(unhighlightNode());
        },
        onMouseOut: () => dispatch(unhighlightNode()),
        onMouseOver: () => dispatch(highlightNode(nodeFront)),
      },
      dom.span({ className: "flex-item-index" }, index),
      getNodeRep(nodeFront)
    );
  }
}

module.exports = FlexItem;
