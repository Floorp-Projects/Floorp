/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const GripMessageBody = createFactory(
  require("devtools/client/webconsole/components/Output/GripMessageBody")
);

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);

loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/index").MODE;
});

const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);

SimpleTable.displayName = "SimpleTable";

SimpleTable.propTypes = {
  columns: PropTypes.object.isRequired,
  items: PropTypes.array.isRequired,
  dispatch: PropTypes.func.isRequired,
  serviceContainer: PropTypes.object.isRequired,
};

function SimpleTable(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
    badge,
    open,
  } = props;

  const {
    source,
    type,
    level,
    id: messageId,
    indent,
    timeStamp,
    columns,
    items,
  } = message;

  // if we don't have any data, don't show anything.
  if (!items.length) {
    return null;
  }
  const headerItems = [];
  columns.forEach((value, key) =>
    headerItems.push(
      dom.th(
        {
          key,
          title: value,
        },
        value
      )
    )
  );

  const rowItems = items.map((item, index) => {
    const cells = [];

    columns.forEach((_, key) => {
      const cellValue = item[key];
      const cellContent =
        typeof cellValue === "undefined"
          ? ""
          : GripMessageBody({
              grip: cellValue,
              mode: MODE.SHORT,
              useQuotes: false,
              serviceContainer,
              dispatch,
            });

      cells.push(
        dom.td(
          {
            key,
          },
          cellContent
        )
      );
    });
    return dom.tr({ key: index }, cells);
  });

  const attachment = dom.table(
    {
      className: "simple-table",
      role: "grid",
    },
    dom.thead({}, dom.tr({ className: "simple-table-header" }, headerItems)),
    dom.tbody({}, rowItems)
  );

  const topLevelClasses = ["cm-s-mozilla"];
  return Message({
    attachment,
    badge,
    dispatch,
    indent,
    level,
    messageId,
    open,
    serviceContainer,
    source,
    timeStamp,
    timestampsVisible,
    topLevelClasses,
    type,
    message,
    messageBody: [],
  });
}

module.exports = SimpleTable;
