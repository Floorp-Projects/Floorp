/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const NetInfoParams = React.createFactory(require("./net-info-params"));

// Shortcuts
const DOM = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This template represents a group of data within a tab. For example,
 * Headers tab has two groups 'Request Headers' and 'Response Headers'
 * The Response tab can also have two groups 'Raw Data' and 'JSON'
 */
var NetInfoGroup = React.createClass({
  propTypes: {
    type: PropTypes.string.isRequired,
    name: PropTypes.string.isRequired,
    params: PropTypes.array,
    content: PropTypes.element,
    open: PropTypes.bool
  },

  displayName: "NetInfoGroup",

  getDefaultProps() {
    return {
      open: true,
    };
  },

  getInitialState() {
    return {
      open: this.props.open,
    };
  },

  onToggle(event) {
    this.setState({
      open: !this.state.open
    });
  },

  render() {
    let content = this.props.content;

    if (!content && this.props.params) {
      content = NetInfoParams({
        params: this.props.params
      });
    }

    let open = this.state.open;
    let className = open ? "opened" : "";

    return (
      DOM.div({className: "netInfoGroup" + " " + className + " " +
        this.props.type},
        DOM.span({
          className: "netInfoGroupTwisty",
          onClick: this.onToggle
        }),
        DOM.span({
          className: "netInfoGroupTitle",
          onClick: this.onToggle},
          this.props.name
        ),
        DOM.div({className: "netInfoGroupContent"},
          content
        )
      )
    );
  }
});

// Exports from this module
module.exports = NetInfoGroup;
