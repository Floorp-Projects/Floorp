/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import React from "react";

export class SafeAnchor extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick(event) {
    // Use dispatch instead of normal link click behavior to include referrer
    if (this.props.dispatch) {
      event.preventDefault();
      const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
      this.props.dispatch(
        ac.OnlyToMain({
          type: at.OPEN_LINK,
          data: {
            event: { altKey, button, ctrlKey, metaKey, shiftKey },
            referrer: "https://getpocket.com/recommendations",
            // Use the anchor's url, which could have been cleaned up
            url: event.currentTarget.href,
          },
        })
      );
    }

    // Propagate event if there's a handler
    if (this.props.onLinkClick) {
      this.props.onLinkClick(event);
    }
  }

  safeURI(url) {
    let protocol = null;
    try {
      protocol = new URL(url).protocol;
    } catch (e) {
      return "";
    }

    const isAllowed = ["http:", "https:"].includes(protocol);
    if (!isAllowed) {
      console.warn(`${url} is not allowed for anchor targets.`); // eslint-disable-line no-console
      return "";
    }
    return url;
  }

  render() {
    const { url, className } = this.props;
    return (
      <a href={this.safeURI(url)} className={className} onClick={this.onClick}>
        {this.props.children}
      </a>
    );
  }
}
