/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { li, div, ul } from "react-dom-factories";
import PropTypes from "prop-types";

import AccessibleImage from "./AccessibleImage";

const classnames = require("devtools/client/shared/classnames.js");

import "./ResultList.css";

export default class ResultList extends Component {
  static defaultProps = {
    size: "small",
    role: "listbox",
  };

  static get propTypes() {
    return {
      items: PropTypes.array.isRequired,
      role: PropTypes.oneOf(["listbox"]),
      selectItem: PropTypes.func.isRequired,
      selected: PropTypes.number.isRequired,
      size: PropTypes.oneOf(["big", "small"]),
    };
  }

  renderListItem = (item, index) => {
    if (item.value === "/" && item.title === "") {
      item.title = "(index)";
    }

    const { selectItem, selected } = this.props;
    const props = {
      onClick: event => selectItem(event, item, index),
      key: `${item.id}${item.value}${index}`,
      ref: React.createRef(),
      title: item.value,
      "aria-labelledby": `${item.id}-title`,
      "aria-describedby": `${item.id}-subtitle`,
      role: "option",
      className: classnames("result-item", {
        selected: index === selected,
      }),
    };

    return li(
      props,
      item.icon &&
        div(
          {
            className: "icon",
          },
          React.createElement(AccessibleImage, {
            className: item.icon,
          })
        ),
      div(
        {
          id: `${item.id}-title`,
          className: "title",
        },
        item.title
      ),
      item.subtitle != item.title
        ? div(
            {
              id: `${item.id}-subtitle`,
              className: "subtitle",
            },
            item.subtitle
          )
        : null
    );
  };
  render() {
    const { size, items, role } = this.props;
    return ul(
      {
        className: classnames("result-list", size),
        id: "result-list",
        role: role,
        "aria-live": "polite",
      },
      items.map(this.renderListItem)
    );
  }
}
