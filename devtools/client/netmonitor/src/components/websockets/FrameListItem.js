/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

loader.lazyGetter(this, "FrameListColumnSize", function() {
  return createFactory(require("./FrameListColumnSize"));
});
loader.lazyGetter(this, "FrameListColumnData", function() {
  return createFactory(require("./FrameListColumnData"));
});
loader.lazyGetter(this, "FrameListColumnOpCode", function() {
  return createFactory(require("./FrameListColumnOpCode"));
});
loader.lazyGetter(this, "FrameListColumnMaskBit", function() {
  return createFactory(require("./FrameListColumnMaskBit"));
});
loader.lazyGetter(this, "FrameListColumnFinBit", function() {
  return createFactory(require("./FrameListColumnFinBit"));
});
loader.lazyGetter(this, "FrameListColumnTime", function() {
  return createFactory(require("./FrameListColumnTime"));
});

const COLUMN_COMPONENT_MAP = {
  time: FrameListColumnTime,
  data: FrameListColumnData,
  size: FrameListColumnSize,
  opCode: FrameListColumnOpCode,
  maskBit: FrameListColumnMaskBit,
  finBit: FrameListColumnFinBit,
};

/**
 * Renders one row in the frame list.
 */
class FrameListItem extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      isSelected: PropTypes.bool.isRequired,
      onMouseDown: PropTypes.func.isRequired,
      connector: PropTypes.object.isRequired,
      visibleColumns: PropTypes.array.isRequired,
    };
  }

  render() {
    const {
      item,
      index,
      isSelected,
      onMouseDown,
      connector,
      visibleColumns,
    } = this.props;

    const classList = [
      "ws-frame-list-item",
      index % 2 ? "odd" : "even",
      item.type,
    ];
    if (isSelected) {
      classList.push("selected");
    }

    return dom.tr(
      {
        className: classList.join(" "),
        tabIndex: 0,
        onMouseDown,
      },
      visibleColumns.map(name => {
        const ColumnComponent = COLUMN_COMPONENT_MAP[name];
        return ColumnComponent({
          key: `ws-frame-list-column-${name}-${index}`,
          connector,
          item,
        });
      })
    );
  }
}

module.exports = FrameListItem;
