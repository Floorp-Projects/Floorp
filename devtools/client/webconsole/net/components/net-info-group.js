/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory, DOM, PropTypes } =
  require("devtools/client/shared/vendor/react");
const NetInfoParams = createFactory(require("./net-info-params"));

/**
 * This template represents a group of data within a tab. For example,
 * Headers tab has two groups 'Request Headers' and 'Response Headers'
 * The Response tab can also have two groups 'Raw Data' and 'JSON'
 */
class NetInfoGroup extends Component {
  static get propTypes() {
    return {
      type: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired,
      params: PropTypes.array,
      content: PropTypes.element,
      open: PropTypes.bool
    };
  }

  static get defaultProps() {
    return {
      open: true,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      open: props.open,
    };

    this.onToggle = this.onToggle.bind(this);
  }

  onToggle(event) {
    this.setState({
      open: !this.state.open
    });
  }

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
}

// Exports from this module
module.exports = NetInfoGroup;
