/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import React from "react";

export class DSEmptyState extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onReset = this.onReset.bind(this);
    this.state = {};
  }

  componentWillUnmount() {
    if (this.timeout) {
      clearTimeout(this.timeout);
    }
  }

  onReset() {
    if (this.props.dispatch && this.props.feed) {
      const { feed } = this.props;
      const { url } = feed;
      this.props.dispatch({
        type: at.DISCOVERY_STREAM_FEED_UPDATE,
        data: {
          feed: {
            ...feed,
            data: {
              ...feed.data,
              status: "waiting",
            },
          },
          url,
        },
      });

      this.setState({ waiting: true });
      this.timeout = setTimeout(() => {
        this.timeout = null;
        this.setState({
          waiting: false,
        });
      }, 300);

      this.props.dispatch(
        ac.OnlyToMain({ type: at.DISCOVERY_STREAM_RETRY_FEED, data: { feed } })
      );
    }
  }

  renderButton() {
    if (this.props.status === "waiting" || this.state.waiting) {
      return (
        <button
          className="try-again-button waiting"
          data-l10n-id="newtab-discovery-empty-section-topstories-loading"
        />
      );
    }

    return (
      <button
        className="try-again-button"
        onClick={this.onReset}
        data-l10n-id="newtab-discovery-empty-section-topstories-try-again-button"
      />
    );
  }

  renderState() {
    if (this.props.status === "waiting" || this.props.status === "failed") {
      return (
        <React.Fragment>
          <h2 data-l10n-id="newtab-discovery-empty-section-topstories-timed-out" />
          {this.renderButton()}
        </React.Fragment>
      );
    }

    return (
      <React.Fragment>
        <h2 data-l10n-id="newtab-discovery-empty-section-topstories-header" />
        <p data-l10n-id="newtab-discovery-empty-section-topstories-content" />
      </React.Fragment>
    );
  }

  render() {
    return (
      <div className="section-empty-state">
        <div className="empty-state-message">{this.renderState()}</div>
      </div>
    );
  }
}
