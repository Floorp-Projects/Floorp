/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { form, div, input } from "react-dom-factories";
import PropTypes from "prop-types";
const classnames = require("devtools/client/shared/classnames.js");

import "./OutlineFilter.css";

export default class OutlineFilter extends Component {
  state = { focused: false };

  static get propTypes() {
    return {
      filter: PropTypes.string.isRequired,
      updateFilter: PropTypes.func.isRequired,
    };
  }

  setFocus = shouldFocus => {
    this.setState({ focused: shouldFocus });
  };

  onChange = e => {
    this.props.updateFilter(e.target.value);
  };

  onKeyDown = e => {
    if (e.key === "Escape" && this.props.filter !== "") {
      // use preventDefault to override toggling the split-console which is
      // also bound to the ESC key
      e.preventDefault();
      this.props.updateFilter("");
    } else if (e.key === "Enter") {
      // We must prevent the form submission from taking any action
      // https://github.com/firefox-devtools/debugger/pull/7308
      e.preventDefault();
    }
  };

  render() {
    const { focused } = this.state;
    return div(
      {
        className: "outline-filter",
      },
      form(
        null,
        input({
          className: classnames("outline-filter-input devtools-filterinput", {
            focused,
          }),
          onFocus: () => this.setFocus(true),
          onBlur: () => this.setFocus(false),
          placeholder: L10N.getStr("outline.placeholder"),
          value: this.props.filter,
          type: "text",
          onChange: this.onChange,
          onKeyDown: this.onKeyDown,
        })
      )
    );
  }
}
