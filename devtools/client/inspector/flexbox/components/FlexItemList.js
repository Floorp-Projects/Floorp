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
const {
  getStr,
} = require("resource://devtools/client/inspector/layout/utils/l10n.js");

const FlexItem = createFactory(
  require("resource://devtools/client/inspector/flexbox/components/FlexItem.js")
);

const Types = require("resource://devtools/client/inspector/flexbox/types.js");

class FlexItemList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      flexItems: PropTypes.arrayOf(PropTypes.shape(Types.flexItem)).isRequired,
      scrollToTop: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const { dispatch, flexItems, scrollToTop, setSelectedNode } = this.props;

    return dom.div(
      { className: "flex-item-list" },
      dom.div(
        {
          className: "flex-item-list-header",
          role: "heading",
          "aria-level": "3",
        },
        !flexItems.length
          ? getStr("flexbox.noFlexItems")
          : getStr("flexbox.flexItems")
      ),
      flexItems.map((flexItem, index) =>
        FlexItem({
          key: flexItem.actorID,
          dispatch,
          flexItem,
          index: index + 1,
          scrollToTop,
          setSelectedNode,
        })
      )
    );
  }
}

module.exports = FlexItemList;
