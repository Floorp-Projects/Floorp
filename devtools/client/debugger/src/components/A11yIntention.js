/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import PropTypes from "prop-types";
import "./A11yIntention.css";

export default class A11yIntention extends React.Component {
  static get propTypes() {
    return {
      children: PropTypes.array.isRequired,
    };
  }

  state = { keyboard: false };

  handleKeyDown = () => {
    this.setState({ keyboard: true });
  };

  handleMouseDown = () => {
    this.setState({ keyboard: false });
  };

  render() {
    return (
      <div
        className={this.state.keyboard ? "A11y-keyboard" : "A11y-mouse"}
        onKeyDown={this.handleKeyDown}
        onMouseDown={this.handleMouseDown}
      >
        {this.props.children}
      </div>
    );
  }
}
