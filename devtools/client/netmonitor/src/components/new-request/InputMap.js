/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createRef } = require("devtools/client/shared/vendor/react");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div, input, textarea, button } = dom;

const CUSTOM_NEW_REQUEST_INPUT_NAME = L10N.getStr(
  "netmonitor.custom.placeholder.name"
);

const CUSTOM_NEW_REQUEST_INPUT_VALUE = L10N.getStr(
  "netmonitor.custom.placeholder.value"
);

const REMOVE_ITEM = L10N.getStr("netmonitor.custom.removeItem");

/**
 * Editable name and value list component with optional form to add new items
 **/
class InputMap extends Component {
  static get propTypes() {
    return {
      list: PropTypes.arrayOf(
        PropTypes.shape({
          name: PropTypes.string.isRequired,
          value: PropTypes.string.isRequired,
          disabled: PropTypes.bool,
        })
      ).isRequired,
      onUpdate: PropTypes.func,
      onAdd: PropTypes.func,
      onDelete: PropTypes.func,
      onChange: PropTypes.func,
      onChecked: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    this.listRef = createRef();

    this.state = {
      name: "",
      value: "",
    };
  }

  render() {
    const { list, onUpdate, onAdd, onDelete, onChecked } = this.props;
    const { name, value } = this.state;

    // Adds a new item with name and value when the user starts typing on the form
    const onKeyDown = event => {
      const { target } = event;
      onAdd(name, value);
      this.setState({ name: "", value: "" }, () => {
        // Get next to last child on the list,
        // because that was the item that was just added and
        // we need to focous on it, so the user can keep editing it.
        const targetParentNode = this.listRef.current.childNodes?.[
          this.listRef.current.childElementCount - 2
        ];
        targetParentNode?.querySelector(`.${target.className}`).focus();
      });
    };

    return div(
      {
        ref: this.listRef,
        className: "http-custom-input-and-map-form",
      },
      list.map((item, index) => {
        return div(
          {
            className: "tabpanel-summary-container http-custom-input",
            id: `http-custom-${item.name.toLowerCase()}`,
            key: index,
          },
          input({
            className: "tabpanel-summary-input-checkbox",
            name: `checked-${index}`,
            type: "checkbox",
            onChange: event => {
              onChecked(index, event.target.checked);
            },
            checked: item.checked,
            disabled: !!item.disabled,
            wrap: "off",
          }),
          div(
            { className: "tabpanel-summary-input-name" },
            div(
              {
                className: "auto-growing-textarea",
                "data-replicated-value": item.name,
                title: item.name,
              },
              textarea({
                className: "http-custom-input-name",
                name: `name-${index}`,
                value: item.name,
                disabled: !!item.disabled,
                onChange: event => {
                  onUpdate(event);
                },
                rows: 1,
              })
            )
          ),
          div(
            { className: "tabpanel-summary-input-value" },
            div(
              {
                className: "auto-growing-textarea",
                "data-replicated-value": item.value,
                title: item.value,
              },
              textarea({
                className: "http-custom-input-value",
                name: `value-${index}`,
                placeholder: "value",
                disabled: !!item.disabled,
                onChange: event => {
                  onUpdate(event);
                },
                value: item.value,
                rows: 1,
              })
            )
          ),
          !item.disabled &&
            onDelete &&
            button({
              className: "http-custom-delete-button",
              title: REMOVE_ITEM,
              "aria-label": REMOVE_ITEM,
              onClick: () => onDelete(index),
            })
        );
      }),
      onAdd &&
        div(
          {
            className: "map-add-new-inputs",
          },
          input({
            className: "tabpanel-summary-input-checkbox",
            onChange: () => {},
            checked: true,
            type: "checkbox",
          }),
          div(
            { className: "tabpanel-summary-input-name" },
            div(
              {
                className: "auto-growing-textarea",
                "data-replicated-value": name,
                title: value,
              },
              textarea({
                className: "http-custom-input-name",
                type: "text",
                ref: "addInputName",
                checked: true,
                value: name,
                rows: 1,
                placeholder: CUSTOM_NEW_REQUEST_INPUT_NAME,
                onChange: e => this.setState({ name: e.target.value }),
                onKeyDown,
              })
            )
          ),
          div(
            { className: "tabpanel-summary-input-value" },
            div(
              {
                className: "auto-growing-textarea",
                "data-replicated-value": value,
                title: value,
              },
              textarea({
                className: "http-custom-input-value",
                type: "text",
                ref: "addInputValue",
                value,
                onChange: e => this.setState({ value: e.target.value }),
                rows: 1,
                placeholder: CUSTOM_NEW_REQUEST_INPUT_VALUE,
                onKeyDown,
              })
            )
          )
        )
    );
  }
}

module.exports = InputMap;
