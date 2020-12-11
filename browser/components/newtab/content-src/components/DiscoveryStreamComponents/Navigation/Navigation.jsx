/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";

export class Topic extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.UserEvent({
          event: "CLICK",
          source: "POPULAR_TOPICS",
          action_position: 0,
          value: {
            topic: event.target.text.toLowerCase().replace(` `, `-`),
          },
        })
      );
    }
  }

  render() {
    const { url, name } = this.props;
    return (
      <SafeAnchor
        onLinkClick={this.onLinkClick}
        className={this.props.className}
        url={url}
      >
        {name}
      </SafeAnchor>
    );
  }
}

export class Navigation extends React.PureComponent {
  render() {
    const links = this.props.links || [];
    const alignment = this.props.alignment || "centered";
    const header = this.props.header || {};
    return (
      <div className={`ds-navigation ds-navigation-${alignment}`}>
        {header.title ? (
          <FluentOrText message={header.title}>
            <span className="ds-navigation-header" />
          </FluentOrText>
        ) : null}
        <ul>
          {links &&
            links.map(t => (
              <li key={t.name}>
                <Topic
                  url={t.url}
                  name={t.name}
                  dispatch={this.props.dispatch}
                />
              </li>
            ))}
        </ul>
      </div>
    );
  }
}
