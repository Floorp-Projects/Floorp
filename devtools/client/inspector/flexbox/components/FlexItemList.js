/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FlexItem = createFactory(require(("./FlexItem")));

const Types = require("../types");

class FlexItemList extends PureComponent {
  static get propTypes() {
    return {
      flexItems: PropTypes.arrayOf(PropTypes.shape(Types.flexItem)).isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      flexItems,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      setSelectedNode,
    } = this.props;

    return (
      dom.ol(
        { className: "flex-item-list" },
        flexItems.map(flexItem => FlexItem({
          key: flexItem.actorID,
          flexItem,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          setSelectedNode,
        }))
      )
    );
  }
}

module.exports = FlexItemList;
