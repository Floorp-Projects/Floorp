/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file should not be modified and is a duplicate from the debugger.html project.
 * Any changes to this file should be imported from the upstream debugger.html project.
 */

"use strict";

const {
  createElement,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { div, span } = dom;

class Accordion extends PureComponent {
  static get propTypes() {
    return {
      items: PropTypes.array,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      created: {},
      opened: Object.fromEntries(
        props.items.map(item => [item.header, item.opened])
      ),
    };

    this.handleHeaderClick = this.handleHeaderClick.bind(this);
    this.renderContainer = this.renderContainer.bind(this);
  }

  handleHeaderClick(event, item) {
    event.stopPropagation();
    const isOpened = !this.isOpened(item);

    if (isOpened && item.onOpened) {
      item.onOpened();
    }

    if (item.onToggled) {
      item.onToggled();
    }

    this.setState({
      created: {
        ...this.state.created,
        [item.header]: true,
      },
      opened: {
        ...this.state.opened,
        [item.header]: isOpened,
      },
    });
  }

  isOpened(item) {
    const isOpened = this.state.opened[item.header];
    return typeof isOpened === "boolean" ? isOpened : item.opened;
  }

  renderContainer(item) {
    const isOpened = this.isOpened(item);
    const isCreated = this.state.created[item.header] === true;
    const containerClassName = item.className
      ? item.className
      : item.header.toLowerCase().replace(/\s/g, "-") + "-pane";

    return div(
      { className: containerClassName, key: item.header },

      div(
        {
          className: "_header",
          onClick: event => this.handleHeaderClick(event, item),
        },
        span({ className: `arrow theme-twisty${isOpened ? " open" : ""}` }),
        span({ className: "truncate" }, item.header)
      ),

      isCreated || isOpened
        ? div(
            {
              className: "_content",
              style: { display: isOpened ? "block" : "none" },
            },
            createElement(item.component, item.componentProps || {})
          )
        : null
    );
  }

  render() {
    return div(
      { className: "accordion" },
      this.props.items.map(this.renderContainer)
    );
  }
}

module.exports = Accordion;
