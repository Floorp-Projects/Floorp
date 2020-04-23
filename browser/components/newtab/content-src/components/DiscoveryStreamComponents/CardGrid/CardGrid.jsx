/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DSCard, PlaceholderDSCard } from "../DSCard/DSCard.jsx";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState.jsx";
import React from "react";

export class CardGrid extends React.PureComponent {
  renderCards() {
    const recs = this.props.data.recommendations.slice(0, this.props.items);
    const cards = [];

    for (let index = 0; index < this.props.items; index++) {
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
            title={rec.title}
            excerpt={rec.excerpt}
            url={rec.url}
            id={rec.id}
            shim={rec.shim}
            type={this.props.type}
            context={rec.context}
            sponsor={rec.sponsor}
            dispatch={this.props.dispatch}
            source={rec.domain}
            pocket_id={rec.pocket_id}
            context_type={rec.context_type}
            bookmarkGuid={rec.bookmarkGuid}
            engagement={rec.engagement}
            display_engagement_labels={this.props.display_engagement_labels}
            cta={rec.cta}
            cta_variant={this.props.cta_variant}
          />
        )
      );
    }

    // Used for CSS overrides to default styling (eg: "hero")
    const variantClass = this.props.display_variant
      ? `ds-card-grid-${this.props.display_variant}`
      : ``;

    return (
      <div
        className={`ds-card-grid ds-card-grid-${this.props.border} ${variantClass}`}
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
              <div className="ds-context">{this.props.context}</div>
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
      </div>
    );
  }
}

CardGrid.defaultProps = {
  border: `border`,
  items: 4, // Number of stories to display
};
