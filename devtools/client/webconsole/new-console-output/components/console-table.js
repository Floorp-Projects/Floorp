/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createClass,
  createFactory,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { ObjectClient } = require("devtools/shared/client/main");
const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const {l10n} = require("devtools/client/webconsole/new-console-output/utils/messages");
const GripMessageBody = createFactory(require("devtools/client/webconsole/new-console-output/components/grip-message-body"));

const TABLE_ROW_MAX_ITEMS = 1000;
const TABLE_COLUMN_MAX_ITEMS = 10;

const ConsoleTable = createClass({

  displayName: "ConsoleTable",

  propTypes: {
    dispatch: PropTypes.func.isRequired,
    parameters: PropTypes.array.isRequired,
    serviceContainer: PropTypes.shape({
      hudProxyClient: PropTypes.object.isRequired,
    }),
    id: PropTypes.string.isRequired,
  },

  componentWillMount: function () {
    const {id, dispatch, serviceContainer, parameters} = this.props;

    if (!Array.isArray(parameters) || parameters.length === 0) {
      return;
    }

    const client = new ObjectClient(serviceContainer.hudProxyClient, parameters[0]);
    let dataType = getParametersDataType(parameters);

    // Get all the object properties.
    dispatch(actions.messageTableDataGet(id, client, dataType));
  },

  getHeaders: function (columns) {
    let headerItems = [];
    columns.forEach((value, key) => headerItems.push(dom.th({}, value)));
    return headerItems;
  },

  getRows: function (columns, items) {
    return items.map(item => {
      let cells = [];
      columns.forEach((value, key) => {
        cells.push(
          dom.td(
            {},
            GripMessageBody({
              grip: item[key],
              mode: "short",
            })
          )
        );
      });
      return dom.tr({}, cells);
    });
  },

  render: function () {
    const {parameters, tableData} = this.props;
    const headersGrip = parameters[1];
    const headers = headersGrip && headersGrip.preview ? headersGrip.preview.items : null;

    // if tableData is nullable, we don't show anything.
    if (!tableData) {
      return null;
    }

    const {columns, items} = getTableItems(
      tableData,
      getParametersDataType(parameters),
      headers
    );

    return (
      dom.table({className: "new-consoletable devtools-monospace"},
        dom.thead({}, this.getHeaders(columns)),
        dom.tbody({}, this.getRows(columns, items))
      )
    );
  }
});

function getParametersDataType(parameters = null) {
  if (!Array.isArray(parameters) || parameters.length === 0) {
    return null;
  }
  return parameters[0].class;
}

function getTableItems(data = {}, type, headers = null) {
  const INDEX_NAME = "_index";
  const VALUE_NAME = "_value";
  const namedIndexes = {
    [INDEX_NAME]: (
      ["Object", "Array"].includes(type) ?
        l10n.getStr("table.index") : l10n.getStr("table.iterationIndex")
    ),
    [VALUE_NAME]: l10n.getStr("table.value"),
    key: l10n.getStr("table.key")
  };

  let columns = new Map();
  let items = [];

  let addItem = function (item) {
    items.push(item);
    Object.keys(item).forEach(key => addColumn(key));
  };

  let addColumn = function (columnIndex) {
    let columnExists = columns.has(columnIndex);
    let hasMaxColumns = columns.size == TABLE_COLUMN_MAX_ITEMS;
    let hasCustomHeaders = Array.isArray(headers);

    if (
      !columnExists &&
      !hasMaxColumns && (
        !hasCustomHeaders ||
        headers.includes(columnIndex) ||
        columnIndex === INDEX_NAME
      )
    ) {
      columns.set(columnIndex, namedIndexes[columnIndex] || columnIndex);
    }
  };

  for (let index of Object.keys(data)) {
    if (type !== "Object" && index == parseInt(index, 10)) {
      index = parseInt(index, 10);
    }

    let item = {
      [INDEX_NAME]: index
    };

    let property = data[index].value;

    if (property.preview) {
      let {preview} = property;
      let entries = preview.ownProperties || preview.items;
      if (entries) {
        for (let key of Object.keys(entries)) {
          let entry = entries[key];
          item[key] = entry.value || entry;
        }
      } else {
        if (preview.key) {
          item.key = preview.key;
        }

        item[VALUE_NAME] = preview.value || property;
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
  if (Array.isArray(headers)) {
    headers.forEach(header => addColumn(header));
  }

  // We want to always have the index column first
  if (columns.has(INDEX_NAME)) {
    let index = columns.get(INDEX_NAME);
    columns.delete(INDEX_NAME);
    columns = new Map([[INDEX_NAME, index], ...columns.entries()]);
  }

  // We want to always have the values column last
  if (columns.has(VALUE_NAME)) {
    let index = columns.get(VALUE_NAME);
    columns.delete(VALUE_NAME);
    columns.set(VALUE_NAME, index);
  }

  return {
    columns,
    items
  };
}

module.exports = ConsoleTable;
