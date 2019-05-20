/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import classnames from "classnames";

import "./OutlineFilter.css";

type Props = {
  filter: string,
  updateFilter: (filter: string) => void,
};

type State = {
  focused: boolean,
};

export default class OutlineFilter extends Component<Props, State> {
  state = { focused: false };

  setFocus = (shouldFocus: boolean) => {
    this.setState({ focused: shouldFocus });
  };

  onChange = (e: SyntheticInputEvent<HTMLElement>) => {
    this.props.updateFilter(e.target.value);
  };

  onKeyDown = (e: SyntheticKeyboardEvent<HTMLInputElement>) => {
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
    return (
      <div className="outline-filter">
        <form>
          <input
            className={classnames("outline-filter-input", { focused })}
            onFocus={() => this.setFocus(true)}
            onBlur={() => this.setFocus(false)}
            placeholder={L10N.getStr("outline.placeholder")}
            value={this.props.filter}
            type="text"
            onChange={this.onChange}
            onKeyDown={this.onKeyDown}
          />
        </form>
      </div>
    );
  }
}
