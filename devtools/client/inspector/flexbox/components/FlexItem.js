/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

loader.lazyRequireGetter(
  this,
  "getNodeRep",
  "resource://devtools/client/inspector/shared/node-reps.js"
);

const Types = require("resource://devtools/client/inspector/flexbox/types.js");

const {
  highlightNode,
  unhighlightNode,
} = require("resource://devtools/client/inspector/boxmodel/actions/box-model-highlighter.js");

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
    const { dispatch, flexItem, index, scrollToTop, setSelectedNode } =
      this.props;
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
