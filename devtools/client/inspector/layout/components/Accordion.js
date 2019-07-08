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
      opened: props.items.map(item => item.opened),
      created: [],
    };

    this.handleHeaderClick = this.handleHeaderClick.bind(this);
    this.renderContainer = this.renderContainer.bind(this);
  }

  handleHeaderClick(i, event) {
    const opened = [...this.state.opened];
    const created = [...this.state.created];
    const item = this.props.items[i];

    event.stopPropagation();

    opened[i] = !opened[i];
    created[i] = true;

    if (opened[i] && item.onOpened) {
      item.onOpened();
    }

    if (item.onToggled) {
      item.onToggled();
    }

    this.setState({ opened, created });
  }

  renderContainer(item, i) {
    const { opened, created } = this.state;
    const containerClassName = item.className
      ? item.className
      : item.header.toLowerCase().replace(/\s/g, "-") + "-pane";
    let arrowClassName = "arrow theme-twisty";
    if (opened[i]) {
      arrowClassName += " open";
    }

    return div(
      { className: containerClassName, key: i },

      div(
        {
          className: "_header",
          onClick: event => this.handleHeaderClick(i, event),
        },
        span({ className: arrowClassName }),
        span({ className: "truncate" }, item.header)
      ),

      created[i] || opened[i]
        ? div(
            {
              className: "_content",
              style: { display: opened[i] ? "block" : "none" },
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
