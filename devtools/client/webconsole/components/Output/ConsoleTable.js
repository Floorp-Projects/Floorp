/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  l10n,
  getArrayTypeNames,
  getDescriptorValue,
} = require("devtools/client/webconsole/utils/messages");
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/index").MODE;
});

const GripMessageBody = createFactory(
  require("devtools/client/webconsole/components/Output/GripMessageBody")
);

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);

const TABLE_ROW_MAX_ITEMS = 1000;
// Match Chrome max column number.
const TABLE_COLUMN_MAX_ITEMS = 21;

class ConsoleTable extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      parameters: PropTypes.array.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      id: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.getHeaders = this.getHeaders.bind(this);
    this.getRows = this.getRows.bind(this);
  }

  getHeaders(columns) {
    const headerItems = [];
    columns.forEach((value, key) =>
      headerItems.push(
        dom.div(
          {
            className: "new-consoletable-header",
            role: "columnheader",
            key,
            title: value,
          },
          value
        )
      )
    );
    return headerItems;
  }

  getRows(columns, items) {
    const { dispatch, serviceContainer } = this.props;

    return items.map((item, index) => {
      const cells = [];
      const className = index % 2 ? "odd" : "even";

      columns.forEach((value, key) => {
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
          dom.div(
            {
              role: "gridcell",
              className,
              key,
            },
            cellContent
          )
        );
      });
      return cells;
    });
  }

  render() {
    const { parameters } = this.props;
    const { valueGrip, headersGrip } = getValueAndHeadersGrip(parameters);

    const headers = headersGrip?.preview ? headersGrip.preview.items : null;

    const data = valueGrip?.ownProperties;

    // if we don't have any data, don't show anything.
    if (!data) {
      return null;
    }

    const dataType = getParametersDataType(parameters);
    const { columns, items } = getTableItems(data, dataType, headers);

    return dom.div(
      {
        className: "new-consoletable",
        role: "grid",
        style: {
          gridTemplateColumns: `repeat(${columns.size}, calc(100% / ${columns.size}))`,
        },
      },
      this.getHeaders(columns),
      this.getRows(columns, items)
    );
  }
}

function getValueAndHeadersGrip(parameters) {
  const [valueFront, headersFront] = parameters;

  const headersGrip = headersFront?.getGrip
    ? headersFront.getGrip()
    : headersFront;

  const valueGrip = valueFront?.getGrip ? valueFront.getGrip() : valueFront;

  return { valueGrip, headersGrip };
}

function getParametersDataType(parameters = null) {
  if (!Array.isArray(parameters) || parameters.length === 0) {
    return null;
  }
  const [firstParam] = parameters;
  if (!firstParam || !firstParam.getGrip) {
    return null;
  }
  const grip = firstParam.getGrip();
  return grip.class;
}

const INDEX_NAME = "_index";
const VALUE_NAME = "_value";

function getNamedIndexes(type) {
  return {
    [INDEX_NAME]: getArrayTypeNames()
      .concat("Object")
      .includes(type)
      ? l10n.getStr("table.index")
      : l10n.getStr("table.iterationIndex"),
    [VALUE_NAME]: l10n.getStr("table.value"),
    key: l10n.getStr("table.key"),
  };
}

function hasValidCustomHeaders(headers) {
  return (
    Array.isArray(headers) &&
    headers.every(
      header => typeof header === "string" || Number.isInteger(Number(header))
    )
  );
}

function getTableItems(data = {}, type, headers = null) {
  const namedIndexes = getNamedIndexes(type);

  let columns = new Map();
  const items = [];

  const addItem = function(item) {
    items.push(item);
    Object.keys(item).forEach(key => addColumn(key));
  };

  const validCustomHeaders = hasValidCustomHeaders(headers);

  const addColumn = function(columnIndex) {
    const columnExists = columns.has(columnIndex);
    const hasMaxColumns = columns.size == TABLE_COLUMN_MAX_ITEMS;

    if (
      !columnExists &&
      !hasMaxColumns &&
      (!validCustomHeaders ||
        headers.includes(columnIndex) ||
        columnIndex === INDEX_NAME)
    ) {
      columns.set(columnIndex, namedIndexes[columnIndex] || columnIndex);
    }
  };

  for (let [index, property] of Object.entries(data)) {
    if (type !== "Object" && index == parseInt(index, 10)) {
      index = parseInt(index, 10);
    }

    const item = {
      [INDEX_NAME]: index,
    };

    const propertyValue = getDescriptorValue(property);
    const propertyValueGrip = propertyValue?.getGrip
      ? propertyValue.getGrip()
      : propertyValue;

    if (propertyValueGrip?.ownProperties) {
      const entries = propertyValueGrip.ownProperties;
      for (const [key, entry] of Object.entries(entries)) {
        item[key] = getDescriptorValue(entry);
      }
    } else if (
      propertyValueGrip?.preview &&
      (type === "Map" || type === "WeakMap")
    ) {
      item.key = propertyValueGrip.preview.key;
      item[VALUE_NAME] = propertyValueGrip.preview.value;
    } else {
      item[VALUE_NAME] = propertyValue;
    }

    addItem(item);

    if (items.length === TABLE_ROW_MAX_ITEMS) {
      break;
    }
  }

  // Some headers might not be present in the items, so we make sure to
  // return all the headers set by the user.
  if (validCustomHeaders) {
    headers.forEach(header => addColumn(header));
  }

  // We want to always have the index column first
  if (columns.has(INDEX_NAME)) {
    const index = columns.get(INDEX_NAME);
    columns.delete(INDEX_NAME);
    columns = new Map([[INDEX_NAME, index], ...columns.entries()]);
  }

  // We want to always have the values column last
  if (columns.has(VALUE_NAME)) {
    const index = columns.get(VALUE_NAME);
    columns.delete(VALUE_NAME);
    columns.set(VALUE_NAME, index);
  }

  return {
    columns,
    items,
  };
}

module.exports = ConsoleTable;
