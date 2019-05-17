/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import type { Node } from "react";
import "./Dropdown.css";

type Props = {
  panel: React$Element<any>,
  icon: Node,
};

type State = {
  dropdownShown: boolean,
};

export class Dropdown extends Component<Props, State> {
  toggleDropdown: Function;
  constructor(props: Props) {
    super(props);
    this.state = {
      dropdownShown: false,
    };
  }

  toggleDropdown = (e: SyntheticKeyboardEvent<HTMLElement>) => {
    this.setState(prevState => ({
      dropdownShown: !prevState.dropdownShown,
    }));
  };

  renderPanel() {
    return (
      <div
        className="dropdown"
        onClick={this.toggleDropdown}
        style={{ display: this.state.dropdownShown ? "block" : "none" }}
      >
        {this.props.panel}
      </div>
    );
  }

  renderButton() {
    return (
      // eslint-disable-next-line prettier/prettier
      <button className="dropdown-button" onClick={this.toggleDropdown}>
        {this.props.icon}
      </button>
    );
  }

  renderMask() {
    return (
      <div
        className="dropdown-mask"
        onClick={this.toggleDropdown}
        style={{ display: this.state.dropdownShown ? "block" : "none" }}
      />
    );
  }

  render() {
    return (
      <div className="dropdown-block">
        {this.renderPanel()}
        {this.renderButton()}
        {this.renderMask()}
      </div>
    );
  }
}

export default Dropdown;
