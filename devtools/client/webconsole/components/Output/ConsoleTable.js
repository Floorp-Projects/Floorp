/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const actions = require("devtools/client/webconsole/actions/messages");
const {
  l10n,
  getArrayTypeNames,
  getDescriptorValue,
} = require("devtools/client/webconsole/utils/messages");
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
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
      // Remove in Firefox 74. See Bug 1597955.
      tableData: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.getHeaders = this.getHeaders.bind(this);
    this.getRows = this.getRows.bind(this);
  }

  // Remove in Firefox 74. See Bug 1597955.
  componentWillMount() {
    const { id, dispatch, parameters, tableData } = this.props;

    const data = tableData || (parameters[0] && parameters[0].ownProperties);

    if (!Array.isArray(parameters) || parameters.length === 0 || data) {
      return;
    }

    // Get all the object properties.
    dispatch(
      actions.messageGetTableData(
        id,
        parameters[0],
        getParametersDataType(parameters)
      )
    );
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
    const { parameters, tableData } = this.props;
    const [valueGrip, headersGrip] = parameters;
    const headers =
      headersGrip && headersGrip.preview ? headersGrip.preview.items : null;

    const data =
      valueGrip && valueGrip.ownProperties
        ? valueGrip.ownProperties
        : tableData;

    // if we don't have any data, don't show anything.
    if (!data) {
      return null;
    }

    const dataType = getParametersDataType(parameters);

    // Remove deprecatedGetTableItems in Firefox 74. See Bug 1597955.
    const { columns, items } =
      valueGrip && valueGrip.ownProperties
        ? getTableItems(data, dataType, headers)
        : deprecatedGetTableItems(data, dataType, headers);

    return dom.div(
      {
        className: "new-consoletable",
        role: "grid",
        style: {
          gridTemplateColumns: `repeat(${columns.size}, calc(100% / ${
            columns.size
          }))`,
        },
      },
      this.getHeaders(columns),
      this.getRows(columns, items)
    );
  }
}

function getParametersDataType(parameters = null) {
  if (!Array.isArray(parameters) || parameters.length === 0) {
    return null;
  }
  return parameters[0].class;
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

    if (propertyValue && propertyValue.ownProperties) {
      const entries = propertyValue.ownProperties;
      for (const [key, entry] of Object.entries(entries)) {
        item[key] = getDescriptorValue(entry);
      }
    } else if (
      propertyValue &&
      propertyValue.preview &&
      (type === "Map" || type === "WeakMap")
    ) {
      item.key = propertyValue.preview.key;
      item[VALUE_NAME] = propertyValue.preview.value;
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

/**
 * This function is handling console.table packets from pre-Firefox72 servers.
 * Remove in Firefox 74. See Bug 1597955.
 */
function deprecatedGetTableItems(data = {}, type, headers = null) {
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

  for (let index of Object.keys(data)) {
    if (type !== "Object" && index == parseInt(index, 10)) {
      index = parseInt(index, 10);
    }

    const item = {
      [INDEX_NAME]: index,
    };

    const property = data[index] ? data[index].value : undefined;

    if (property && property.preview) {
      const { preview } = property;
      const entries = preview.ownProperties || preview.items;
      if (entries) {
        for (const [key, entry] of Object.entries(entries)) {
          item[key] =
            entry && Object.prototype.hasOwnProperty.call(entry, "value")
              ? entry.value
              : entry;
        }
      } else {
        if (preview.key) {
          item.key = preview.key;
        }

        item[VALUE_NAME] = Object.prototype.hasOwnProperty.call(
          preview,
          "value"
        )
          ? preview.value
          : property;
      }
    } else {
      item[VALUE_NAME] = property;
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
