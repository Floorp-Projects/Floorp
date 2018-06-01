/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ObjectClient = require("devtools/shared/client/object-client");
const actions = require("devtools/client/webconsole/actions/messages");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const { MODE } = require("devtools/client/shared/components/reps/reps");
const GripMessageBody = createFactory(require("devtools/client/webconsole/components/GripMessageBody"));

const TABLE_ROW_MAX_ITEMS = 1000;
const TABLE_COLUMN_MAX_ITEMS = 10;

class ConsoleTable extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      parameters: PropTypes.array.isRequired,
      serviceContainer: PropTypes.shape({
        hudProxy: PropTypes.object.isRequired,
      }),
      id: PropTypes.string.isRequired,
      tableData: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.getHeaders = this.getHeaders.bind(this);
    this.getRows = this.getRows.bind(this);
  }

  componentWillMount() {
    const {id, dispatch, serviceContainer, parameters} = this.props;

    if (!Array.isArray(parameters) || parameters.length === 0) {
      return;
    }

    const client = new ObjectClient(serviceContainer.hudProxy.client, parameters[0]);
    const dataType = getParametersDataType(parameters);

    // Get all the object properties.
    dispatch(actions.messageTableDataGet(id, client, dataType));
  }

  getHeaders(columns) {
    const headerItems = [];
    columns.forEach((value, key) => headerItems.push(dom.div({
      className: "new-consoletable-header",
      role: "columnheader"
    }, value))
  );
    return headerItems;
  }

  getRows(columns, items) {
    const {
      dispatch,
      serviceContainer,
    } = this.props;

    return items.map((item, index) => {
      const cells = [];
      columns.forEach((value, key) => {
        cells.push(
          dom.div(
            {
              role: "gridcell",
              className: (index % 2) ? "odd" : "even"
            },
            GripMessageBody({
              grip: item[key],
              mode: MODE.SHORT,
              useQuotes: false,
              serviceContainer,
              dispatch,
            })
          )
        );
      });
      return cells;
    });
  }

  render() {
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
      dom.div({
        className: "new-consoletable",
        role: "grid",
        style: {
          gridTemplateColumns: `repeat(${columns.size}, auto)`
        }
      },
        this.getHeaders(columns),
        this.getRows(columns, items)
      )
    );
  }
}

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
  const items = [];

  const addItem = function(item) {
    items.push(item);
    Object.keys(item).forEach(key => addColumn(key));
  };

  const addColumn = function(columnIndex) {
    const columnExists = columns.has(columnIndex);
    const hasMaxColumns = columns.size == TABLE_COLUMN_MAX_ITEMS;
    const hasCustomHeaders = Array.isArray(headers);

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

    const item = {
      [INDEX_NAME]: index
    };

    const property = data[index].value;

    if (property.preview) {
      const {preview} = property;
      const entries = preview.ownProperties || preview.items;
      if (entries) {
        for (const key of Object.keys(entries)) {
          const entry = entries[key];
          item[key] = Object.prototype.hasOwnProperty.call(entry, "value")
            ? entry.value
            : entry;
        }
      } else {
        if (preview.key) {
          item.key = preview.key;
        }

        item[VALUE_NAME] = Object.prototype.hasOwnProperty.call(preview, "value")
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
  if (Array.isArray(headers)) {
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
    items
  };
}

module.exports = ConsoleTable;
