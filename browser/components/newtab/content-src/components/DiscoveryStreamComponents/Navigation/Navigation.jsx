/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.mjs";
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
        ac.DiscoveryStreamUserEvent({
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
    let links = this.props.links || [];
    const alignment = this.props.alignment || "centered";
    const header = this.props.header || {};
    const english = this.props.locale.startsWith("en-");
    const privacyNotice = this.props.privacyNoticeURL || {};
    const { newFooterSection } = this.props;
    const className = `ds-navigation ds-navigation-${alignment} ${
      newFooterSection ? `ds-navigation-new-topics` : ``
    }`;
    let { title } = header;
    if (newFooterSection) {
      title = { id: "newtab-pocket-new-topics-title" };
      if (this.props.extraLinks) {
        links = [
          ...links.slice(0, links.length - 1),
          ...this.props.extraLinks,
          links[links.length - 1],
        ];
      }
    }

    return (
      <div className={className}>
        {title && english ? (
          <FluentOrText message={title}>
            <span className="ds-navigation-header" />
          </FluentOrText>
        ) : null}

        {english ? (
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
        ) : null}

        {!newFooterSection ? (
          <SafeAnchor className="ds-navigation-privacy" url={privacyNotice.url}>
            <FluentOrText message={privacyNotice.title} />
          </SafeAnchor>
        ) : null}

        {newFooterSection ? (
          <div className="ds-navigation-family">
            <span className="icon firefox-logo" />
            <span>|</span>
            <span className="icon pocket-logo" />
            <span
              className="ds-navigation-family-message"
              data-l10n-id="newtab-pocket-pocket-firefox-family"
            />
          </div>
        ) : null}
      </div>
    );
  }
}
