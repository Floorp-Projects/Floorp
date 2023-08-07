/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { button, div } from "react-dom-factories";
import PropTypes from "prop-types";
import "./Dropdown.css";

export class Dropdown extends Component {
  constructor(props) {
    super(props);
    this.state = {
      dropdownShown: false,
    };
  }

  static get propTypes() {
    return {
      icon: PropTypes.node.isRequired,
      panel: PropTypes.node.isRequired,
    };
  }

  toggleDropdown = e => {
    this.setState(prevState => ({
      dropdownShown: !prevState.dropdownShown,
    }));
  };

  renderPanel() {
    return div(
      {
        className: "dropdown",
        onClick: this.toggleDropdown,
        style: {
          display: this.state.dropdownShown ? "block" : "none",
        },
      },
      this.props.panel
    );
  }

  renderButton() {
    return button(
      {
        className: "dropdown-button",
        onClick: this.toggleDropdown,
      },
      this.props.icon
    );
  }

  renderMask() {
    return div({
      className: "dropdown-mask",
      onClick: this.toggleDropdown,
      style: {
        display: this.state.dropdownShown ? "block" : "none",
      },
    });
  }
  render() {
    return div(
      {
        className: "dropdown-block",
      },
      this.renderPanel(),
      this.renderButton(),
      this.renderMask()
    );
  }
}

export default Dropdown;
