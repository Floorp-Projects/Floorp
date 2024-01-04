/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class DSDismiss extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onDismissClick = this.onDismissClick.bind(this);
    this.onHover = this.onHover.bind(this);
    this.offHover = this.offHover.bind(this);
    this.state = {
      hovering: false,
    };
  }

  onDismissClick() {
    if (this.props.onDismissClick) {
      this.props.onDismissClick();
    }
  }

  onHover() {
    this.setState({
      hovering: true,
    });
  }

  offHover() {
    this.setState({
      hovering: false,
    });
  }

  render() {
    let className = `ds-dismiss
      ${this.state.hovering ? ` hovering` : ``}
      ${this.props.extraClasses ? ` ${this.props.extraClasses}` : ``}`;

    return (
      <div className={className}>
        {this.props.children}
        <button
          className="ds-dismiss-button"
          data-l10n-id="newtab-dismiss-button-tooltip"
          onClick={this.onDismissClick}
          onMouseEnter={this.onHover}
          onMouseLeave={this.offHover}
        >
          <span className="icon icon-dismiss" />
        </button>
      </div>
    );
  }
}
