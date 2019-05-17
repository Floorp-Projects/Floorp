/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import classnames from "classnames";

import AccessibleImage from "./AccessibleImage";

import "./ResultList.css";

type Props = {
  items: Array<any>,
  selected: number,
  selectItem: (
    event: SyntheticKeyboardEvent<HTMLElement>,
    item: any,
    index: number
  ) => void,
  size: string,
  role: string,
};

export default class ResultList extends Component<Props> {
  displayName: "ResultList";

  static defaultProps = {
    size: "small",
    role: "listbox",
  };

  renderListItem = (item: any, index: number) => {
    if (item.value === "/" && item.title === "") {
      item.title = "(index)";
    }

    const { selectItem, selected } = this.props;
    const props = {
      onClick: event => selectItem(event, item, index),
      key: `${item.id}${item.value}${index}`,
      ref: String(index),
      title: item.value,
      "aria-labelledby": `${item.id}-title`,
      "aria-describedby": `${item.id}-subtitle`,
      role: "option",
      className: classnames("result-item", {
        selected: index === selected,
      }),
    };

    return (
      <li {...props}>
        {item.icon && (
          <div className="icon">
            <AccessibleImage className={item.icon} />
          </div>
        )}
        <div id={`${item.id}-title`} className="title">
          {item.title}
        </div>
        {item.subtitle != item.title ? (
          <div id={`${item.id}-subtitle`} className="subtitle">
            {item.subtitle}
          </div>
        ) : null}
      </li>
    );
  };

  render() {
    const { size, items, role } = this.props;

    return (
      <ul
        className={classnames("result-list", size)}
        id="result-list"
        role={role}
        aria-live="polite"
      >
        {items.map(this.renderListItem)}
      </ul>
    );
  }
}
