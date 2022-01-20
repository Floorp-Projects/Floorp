/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  DSCard,
  PlaceholderDSCard,
  LastCardMessage,
} from "../DSCard/DSCard.jsx";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState.jsx";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";

export class CardGrid extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { moreLoaded: false };
    this.loadMoreClicked = this.loadMoreClicked.bind(this);
  }

  loadMoreClicked() {
    this.props.dispatch(
      ac.UserEvent({
        event: "CLICK",
        source: "DS_LOAD_MORE_BUTTON",
      })
    );
    this.setState({ moreLoaded: true });
  }

  get showLoadMore() {
    const { loadMoreEnabled, data, loadMoreThreshold } = this.props;
    return (
      loadMoreEnabled &&
      data.recommendations.length > loadMoreThreshold &&
      !this.state.moreLoaded
    );
  }

  renderCards() {
    let { items } = this.props;
    const { lastCardMessageEnabled, loadMoreThreshold } = this.props;
    let showLastCardMessage = lastCardMessageEnabled;
    if (this.showLoadMore) {
      items = loadMoreThreshold;
      // We don't want to show this until after load more has been clicked.
      showLastCardMessage = false;
    }

    const recs = this.props.data.recommendations.slice(0, items);
    const cards = [];

    for (let index = 0; index < items; index++) {
      const rec = recs[index];
      cards.push(
        !rec || rec.placeholder ? (
          <PlaceholderDSCard key={`dscard-${index}`} />
        ) : (
          <DSCard
            key={`dscard-${rec.id}`}
            pos={rec.pos}
            flightId={rec.flight_id}
            image_src={rec.image_src}
            raw_image_src={rec.raw_image_src}
            word_count={rec.word_count}
            time_to_read={rec.time_to_read}
            title={rec.title}
            excerpt={rec.excerpt}
            url={rec.url}
            id={rec.id}
            shim={rec.shim}
            type={this.props.type}
            context={rec.context}
            compact={this.props.compact}
            sponsor={rec.sponsor}
            sponsored_by_override={rec.sponsored_by_override}
            dispatch={this.props.dispatch}
            source={rec.domain}
            pocket_id={rec.pocket_id}
            context_type={rec.context_type}
            bookmarkGuid={rec.bookmarkGuid}
            engagement={rec.engagement}
            display_engagement_labels={this.props.display_engagement_labels}
            include_descriptions={this.props.include_descriptions}
            saveToPocketCard={this.props.saveToPocketCard}
            cta={rec.cta}
            cta_variant={this.props.cta_variant}
            is_video={this.props.enable_video_playheads && rec.is_video}
            is_collection={this.props.is_collection}
          />
        )
      );
    }

    // Replace last card with "you are all caught up card"
    if (showLastCardMessage) {
      cards.splice(
        cards.length - 1,
        1,
        <LastCardMessage key={`dscard-last-${cards.length - 1}`} />
      );
    }

    // Used for CSS overrides to default styling (eg: "hero")
    const variantClass = this.props.display_variant
      ? `ds-card-grid-${this.props.display_variant}`
      : ``;

    const compactClass = this.props.compact
      ? `ds-card-grid-compact-variant`
      : ``;

    const includeDescriptions = this.props.include_descriptions
      ? `ds-card-grid-include-descriptions`
      : ``;

    return (
      <div
        className={`ds-card-grid ds-card-grid-${this.props.border} ${variantClass} ${compactClass} ${includeDescriptions}`}
      >
        {cards}
      </div>
    );
  }

  render() {
    const { data } = this.props;

    // Handle a render before feed has been fetched by displaying nothing
    if (!data) {
      return null;
    }

    // Handle the case where a user has dismissed all recommendations
    const isEmpty = data.recommendations.length === 0;

    return (
      <div>
        {this.props.title && (
          <div className="ds-header">
            <div className="title">{this.props.title}</div>
            {this.props.context && (
              <FluentOrText message={this.props.context}>
                <div className="ds-context" />
              </FluentOrText>
            )}
          </div>
        )}
        {isEmpty ? (
          <div className="ds-card-grid empty">
            <DSEmptyState
              status={data.status}
              dispatch={this.props.dispatch}
              feed={this.props.feed}
            />
          </div>
        ) : (
          this.renderCards()
        )}
        {this.showLoadMore && (
          <button
            className="ASRouterButton primary ds-card-grid-load-more-button"
            onClick={this.loadMoreClicked}
            data-l10n-id="newtab-pocket-load-more-stories-button"
          />
        )}
      </div>
    );
  }
}

CardGrid.defaultProps = {
  border: `border`,
  items: 4, // Number of stories to display
  enable_video_playheads: false,
  lastCardMessageEnabled: false,
  saveToPocketCard: false,
  loadMoreThreshold: 12,
};
