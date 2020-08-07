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

class ExploreTopics extends React.PureComponent {
  render() {
    const { explore_topics } = this.props;
    if (!explore_topics) {
      return null;
    }
    return (
      <>
        <Topic
          dispatch={this.props.dispatch}
          className="ds-navigation-inline-explore-more"
          url={explore_topics.url}
          name={explore_topics.name}
        />
        <Topic
          dispatch={this.props.dispatch}
          className="ds-navigation-header-explore-more"
          url={explore_topics.url}
          name={explore_topics.header}
        />
      </>
    );
  }
}

export class Navigation extends React.PureComponent {
  render() {
    const { links } = this.props || [];
    const { alignment } = this.props || "centered";
    // Basic isn't currently used, but keeping it here to be very explicit.
    // The other variant that's supported is "responsive".
    // The responsive variant is intended for longer lists of topics, more than 6.
    // It hides the last item on larger displays. The last item is meant for "see more topics"
    const variant = this.props.display_variant || "basic";
    const header = this.props.header || {};
    const { explore_topics } = this.props;
    return (
      <div
        className={`ds-navigation ds-navigation-${alignment} ds-navigation-variant-${variant}`}
      >
        {header.title ? (
          <FluentOrText message={header.title}>
            <span className="ds-header" />
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
        <ExploreTopics
          dispatch={this.props.dispatch}
          explore_topics={explore_topics}
        />
      </div>
    );
  }
}
