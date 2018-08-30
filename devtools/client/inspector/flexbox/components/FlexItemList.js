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
    };
  }

  render() {
    const { flexItems } = this.props;

    return (
      dom.ol(
        { id: "flex-item-list" },
        flexItems.map(flexItem => FlexItem({
          key: flexItem.actorID,
          flexItem,
        }))
      )
    );
  }
}

module.exports = FlexItemList;
