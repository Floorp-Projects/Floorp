/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file should not be modified and is a duplicate from the debugger.html project.
 * Any changes to this file should be imported from the upstream debugger.html project.
 */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const { DOM: dom, PropTypes } = React;

const { div, span } = dom;

const Accordion = React.createClass({
  displayName: "Accordion",

  propTypes: {
    items: PropTypes.array
  },

  mixins: [ React.addons.PureRenderMixin ],

  getInitialState: function () {
    return { opened: this.props.items.map(item => item.opened),
             created: [] };
  },

  handleHeaderClick: function (i) {
    const opened = [...this.state.opened];
    const created = [...this.state.created];
    const item = this.props.items[i];

    opened[i] = !opened[i];
    created[i] = true;

    if (opened[i] && item.onOpened) {
      item.onOpened();
    }

    this.setState({ opened, created });
  },

  renderContainer: function (item, i) {
    const { opened, created } = this.state;
    const containerClassName =
          item.header.toLowerCase().replace(/\s/g, "-") + "-pane";
    let arrowClassName = "arrow theme-twisty";
    if (opened[i]) {
      arrowClassName += " open";
    }

    return div(
      { className: containerClassName, key: i },

      div(
        { className: "_header",
          onClick: () => this.handleHeaderClick(i) },
        span({ className: arrowClassName }),
        item.header
      ),

      (created[i] || opened[i]) ?
        div(
          { className: "_content",
              style: { display: opened[i] ? "block" : "none" }
          },
          React.createElement(item.component, item.componentProps || {})
        ) :
        null
    );
  },

  render: function () {
    return div(
      { className: "accordion" },
      this.props.items.map(this.renderContainer)
    );
  }
});

module.exports = Accordion;
