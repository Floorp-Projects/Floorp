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

loader.lazyGetter(this, "ColumnData", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnData")
  );
});
loader.lazyGetter(this, "ColumnEventName", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnEventName")
  );
});
loader.lazyGetter(this, "ColumnFinBit", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnFinBit")
  );
});
loader.lazyGetter(this, "ColumnLastEventId", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnLastEventId")
  );
});
loader.lazyGetter(this, "ColumnMaskBit", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnMaskBit")
  );
});
loader.lazyGetter(this, "ColumnOpCode", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnOpCode")
  );
});
loader.lazyGetter(this, "ColumnRetry", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnRetry")
  );
});
loader.lazyGetter(this, "ColumnSize", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnSize")
  );
});
loader.lazyGetter(this, "ColumnTime", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/messages/ColumnTime")
  );
});

const COLUMN_COMPONENT_MAP = {
  data: ColumnData,
  eventName: ColumnEventName,
  finBit: ColumnFinBit,
  lastEventId: ColumnLastEventId,
  maskBit: ColumnMaskBit,
  opCode: ColumnOpCode,
  retry: ColumnRetry,
  size: ColumnSize,
  time: ColumnTime,
};

/**
 * Renders one row in the list.
 */
class MessageListItem extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      isSelected: PropTypes.bool.isRequired,
      onMouseDown: PropTypes.func.isRequired,
      onContextMenu: PropTypes.func.isRequired,
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
      onContextMenu,
      connector,
      visibleColumns,
    } = this.props;

    const classList = [
      "message-list-item",
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
        onContextMenu,
      },
      visibleColumns.map(name => {
        const ColumnComponent = COLUMN_COMPONENT_MAP[name];
        return ColumnComponent({
          key: `message-list-column-${name}-${index}`,
          connector,
          item,
        });
      })
    );
  }
}

module.exports = MessageListItem;
