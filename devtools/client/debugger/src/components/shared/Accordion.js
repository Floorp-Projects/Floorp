/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { cloneElement, Component } from "react";
import { aside, button, div, h2 } from "react-dom-factories";
import PropTypes from "prop-types";

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

  renderContainer = (item, i) => {
    const { opened } = item;
    const contentElementId = `${item.id}-content`;

    return aside(
      {
        className: item.className,
        key: item.id,
        "aria-labelledby": item.id,
        role: item.role,
      },
      h2(
        {
          className: "_header",
        },
        button(
          {
            id: item.id,
            className: "header-label",
            "aria-expanded": `${opened ? "true" : "false"}`,
            "aria-controls": opened ? contentElementId : undefined,
            onClick: () => this.handleHeaderClick(i),
          },
          item.header
        ),
        item.buttons
          ? div(
              {
                className: "header-buttons",
              },
              item.buttons
            )
          : null
      ),
      opened &&
        div(
          {
            className: "_content",
            id: contentElementId,
          },
          cloneElement(item.component, item.componentProps || {})
        )
    );
  };
  render() {
    return div(
      {
        className: "accordion",
      },
      this.props.items.map(this.renderContainer)
    );
  }
}

export default Accordion;
