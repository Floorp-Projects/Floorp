/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { cloneElement, Component } from "react";
import { li, h2, span, div, ul } from "react-dom-factories";
import PropTypes from "prop-types";
import AccessibleImage from "./AccessibleImage";

import "./Accordion.css";

class Accordion extends Component {
  static get propTypes() {
    return {
      items: PropTypes.array.isRequired,
    };
  }

  handleHeaderClick(i) {
    const item = this.props.items[i];
    const opened = !item.opened;
    item.opened = opened;

    if (item.onToggle) {
      item.onToggle(opened);
    }

    // We force an update because otherwise the accordion
    // would not re-render
    this.forceUpdate();
  }

  onHandleHeaderKeyDown(e, i) {
    if (e && (e.key === " " || e.key === "Enter")) {
      this.handleHeaderClick(i);
    }
  }

  renderContainer = (item, i) => {
    const { opened } = item;
    return li(
      {
        className: item.className,
        key: i,
      },
      h2(
        {
          className: "_header",
          tabIndex: "0",
          onKeyDown: e => this.onHandleHeaderKeyDown(e, i),
          onClick: () => this.handleHeaderClick(i),
        },
        React.createElement(AccessibleImage, {
          className: `arrow ${opened ? "expanded" : ""}`,
        }),
        span(
          {
            className: "header-label",
          },
          item.header
        ),
        item.buttons
          ? div(
              {
                className: "header-buttons",
                tabIndex: "-1",
              },
              item.buttons
            )
          : null
      ),
      opened &&
        div(
          {
            className: "_content",
          },
          cloneElement(item.component, item.componentProps || {})
        )
    );
  };
  render() {
    return ul(
      {
        className: "accordion",
      },
      this.props.items.map(this.renderContainer)
    );
  }
}

export default Accordion;
