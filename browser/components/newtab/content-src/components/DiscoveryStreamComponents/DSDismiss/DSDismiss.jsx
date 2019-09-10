/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";

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
    const index = 0;
    const source = "DISCOVERY_STREAM";
    const blockUrlOption = LinkMenuOptions.BlockUrl(
      this.props.data,
      index,
      source
    );

    const { action, impression, userEvent } = blockUrlOption;

    this.props.dispatch(action);
    const userEventData = Object.assign(
      {
        event: userEvent,
        source,
        action_position: index,
      },
      this.props.data
    );
    this.props.dispatch(ac.UserEvent(userEventData));
    if (impression && this.props.shouldSendImpressionStats) {
      this.props.dispatch(impression);
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
          onHover={this.onHover}
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
