/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  DSCard,
  PlaceholderDSCard,
  LastCardMessage,
} from "../DSCard/DSCard.jsx";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState.jsx";
import { TopicsWidget } from "../TopicsWidget/TopicsWidget.jsx";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import React, { useEffect, useState, useRef, useCallback } from "react";
import { connect, useSelector } from "react-redux";
const WIDGET_IDS = {
  TOPICS: 1,
};

export function DSSubHeader(props) {
  return (
    <div className="section-top-bar ds-sub-header">
      <h3 className="section-title-container">
        <span className="section-title">{props.children}</span>
      </h3>
    </div>
  );
}

export function GridContainer(props) {
  const { header, className, children } = props;
  return (
    <>
      {header && (
        <DSSubHeader>
          <FluentOrText message={header} />
        </DSSubHeader>
      )}
      <div className={`ds-card-grid ${className}`}>{children}</div>
    </>
  );
}

export function IntersectionObserver({
  children,
  windowObj = window,
  onIntersecting,
}) {
  const intersectionElement = useRef(null);

  useEffect(() => {
    let observer;
    if (!observer && onIntersecting && intersectionElement.current) {
      observer = new windowObj.IntersectionObserver(entries => {
        const entry = entries.find(e => e.isIntersecting);

        if (entry) {
          // Stop observing since element has been seen
          if (observer && intersectionElement.current) {
            observer.unobserve(intersectionElement.current);
          }

          onIntersecting();
        }
      });
      observer.observe(intersectionElement.current);
    }
    // Cleanup
    return () => observer?.disconnect();
  }, [windowObj, onIntersecting]);

  return <div ref={intersectionElement}>{children}</div>;
}

export function RecentSavesContainer({
  className,
  dispatch,
  windowObj = window,
  items = 3,
}) {
  const { recentSavesData, isUserLoggedIn } = useSelector(
    state => state.DiscoveryStream
  );

  const [visible, setVisible] = useState(false);
  const onIntersecting = useCallback(() => setVisible(true), []);

  useEffect(() => {
    if (visible) {
      dispatch(
        ac.AlsoToMain({
          type: at.DISCOVERY_STREAM_POCKET_STATE_INIT,
        })
      );
    }
  }, [visible, dispatch]);

  // The user has not yet scrolled to this section,
  // so wait before potentially requesting Pocket data.
  if (!visible) {
    return (
      <IntersectionObserver
        windowObj={windowObj}
        onIntersecting={onIntersecting}
      />
    );
  }

  // Intersection observer has finished, but we're not yet logged in.
  if (visible && !isUserLoggedIn) {
    return null;
  }

  function renderCard(rec, index) {
    return (
      <DSCard
        key={`dscard-${rec?.id || index}`}
        id={rec.id}
        pos={index}
        type="CARDGRID_RECENT_SAVES"
        image_src={rec.image_src}
        raw_image_src={rec.raw_image_src}
        word_count={rec.word_count}
        time_to_read={rec.time_to_read}
        title={rec.title}
        excerpt={rec.excerpt}
        url={rec.url}
        source={rec.domain}
        isRecentSave={true}
        dispatch={dispatch}
      />
    );
  }

  const recentSavesCards = [];
  // We fill the cards with a for loop over an inline map because
  // we want empty placeholders if there are not enough cards.
  for (let index = 0; index < items; index++) {
    const recentSave = recentSavesData[index];
    if (!recentSave) {
      recentSavesCards.push(<PlaceholderDSCard key={`dscard-${index}`} />);
    } else {
      recentSavesCards.push(
        renderCard(
          {
            id: recentSave.item_id || recentSave.resolved_id,
            image_src: recentSave.top_image_url,
            raw_image_src: recentSave.top_image_url,
            word_count: recentSave.word_count,
            time_to_read: recentSave.time_to_read,
            title: recentSave.resolved_title,
            url: recentSave.resolved_url,
            domain: recentSave.domain_metadata?.name,
            excerpt: recentSave.excerpt,
          },
          index
        )
      );
    }
  }

  // We are visible and logged in.
  return (
    <GridContainer className={className} header="Recently Saved to your List">
      {recentSavesCards}
    </GridContainer>
  );
}

export class _CardGrid extends React.PureComponent {
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
    const { loadMore, data, loadMoreThreshold } = this.props;
    return (
      loadMore &&
      data.recommendations.length > loadMoreThreshold &&
      !this.state.moreLoaded
    );
  }

  renderCards() {
    let { items } = this.props;
    const { DiscoveryStream } = this.props;
    const { recentSavesEnabled } = DiscoveryStream;
    const {
      hybridLayout,
      hideCardBackground,
      fourCardLayout,
      hideDescriptions,
      lastCardMessageEnabled,
      saveToPocketCard,
      loadMoreThreshold,
      compactGrid,
      compactImages,
      imageGradient,
      newSponsoredLabel,
      titleLines,
      descLines,
      readTime,
      essentialReadsHeader,
      editorsPicksHeader,
      widgets,
    } = this.props;
    let showLastCardMessage = lastCardMessageEnabled;
    if (this.showLoadMore) {
      items = loadMoreThreshold;
      // We don't want to show this until after load more has been clicked.
      showLastCardMessage = false;
    }

    const recs = this.props.data.recommendations.slice(0, items);
    const cards = [];
    let essentialReadsCards = [];
    let editorsPicksCards = [];

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
            displayReadTime={readTime}
            title={rec.title}
            excerpt={rec.excerpt}
            url={rec.url}
            id={rec.id}
            shim={rec.shim}
            type={this.props.type}
            context={rec.context}
            sponsor={rec.sponsor}
            sponsored_by_override={rec.sponsored_by_override}
            dispatch={this.props.dispatch}
            source={rec.domain}
            pocket_id={rec.pocket_id}
            context_type={rec.context_type}
            bookmarkGuid={rec.bookmarkGuid}
            engagement={rec.engagement}
            pocket_button_enabled={this.props.pocket_button_enabled}
            display_engagement_labels={this.props.display_engagement_labels}
            hideDescriptions={hideDescriptions}
            saveToPocketCard={saveToPocketCard}
            compactImages={compactImages}
            imageGradient={imageGradient}
            newSponsoredLabel={newSponsoredLabel}
            titleLines={titleLines}
            descLines={descLines}
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

    if (widgets?.positions?.length && widgets?.data?.length) {
      let positionIndex = 0;
      const source = "CARDGRID_WIDGET";

      for (const widget of widgets.data) {
        let widgetComponent = null;
        const position = widgets.positions[positionIndex];

        // Stop if we run out of positions to place widgets.
        if (!position) {
          break;
        }

        switch (widget?.type) {
          case "TopicsWidget":
            widgetComponent = (
              <TopicsWidget
                position={position.index}
                dispatch={this.props.dispatch}
                source={source}
                id={WIDGET_IDS.TOPICS}
              />
            );
            break;
        }

        if (widgetComponent) {
          // We found a widget, so up the position for next try.
          positionIndex++;
          // We replace an existing card with the widget.
          cards.splice(position.index, 1, widgetComponent);
        }
      }
    }

    let moreRecsHeader = "";
    // For now this is English only.
    if (recentSavesEnabled || (essentialReadsHeader && editorsPicksHeader)) {
      let spliceAt = 6;
      // For 4 card row layouts, second row is 8 cards, and regular it is 6 cards.
      if (fourCardLayout) {
        spliceAt = 8;
      }
      // If we have a custom header, ensure the more recs section also has a header.
      moreRecsHeader = "More Recommendations";
      // Put the first 2 rows into essentialReadsCards.
      essentialReadsCards = [...cards.splice(0, spliceAt)];
      // Put the rest into editorsPicksCards.
      if (essentialReadsHeader && editorsPicksHeader) {
        editorsPicksCards = [...cards.splice(0, cards.length)];
      }
    }

    // Used for CSS overrides to default styling (eg: "hero")
    const variantClass = this.props.display_variant
      ? `ds-card-grid-${this.props.display_variant}`
      : ``;
    const hideCardBackgroundClass = hideCardBackground
      ? `ds-card-grid-hide-background`
      : ``;
    const fourCardLayoutClass = fourCardLayout
      ? `ds-card-grid-four-card-variant`
      : ``;
    const hideDescriptionsClassName = !hideDescriptions
      ? `ds-card-grid-include-descriptions`
      : ``;
    const compactGridClassName = compactGrid ? `ds-card-grid-compact` : ``;
    const hybridLayoutClassName = hybridLayout
      ? `ds-card-grid-hybrid-layout`
      : ``;

    const className = `ds-card-grid-${this.props.border} ${variantClass} ${hybridLayoutClassName} ${hideCardBackgroundClass} ${fourCardLayoutClass} ${hideDescriptionsClassName} ${compactGridClassName}`;

    return (
      <>
        {essentialReadsCards?.length > 0 && (
          <GridContainer className={className}>
            {essentialReadsCards}
          </GridContainer>
        )}
        {recentSavesEnabled && (
          <RecentSavesContainer
            className={className}
            dispatch={this.props.dispatch}
          />
        )}
        {editorsPicksCards?.length > 0 && (
          <GridContainer className={className} header="Editor’s Picks">
            {editorsPicksCards}
          </GridContainer>
        )}
        {cards?.length > 0 && (
          <GridContainer className={className} header={moreRecsHeader}>
            {cards}
          </GridContainer>
        )}
      </>
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

_CardGrid.defaultProps = {
  border: `border`,
  items: 4, // Number of stories to display
  enable_video_playheads: false,
  lastCardMessageEnabled: false,
  saveToPocketCard: false,
  loadMoreThreshold: 12,
};

export const CardGrid = connect(state => ({
  DiscoveryStream: state.DiscoveryStream,
}))(_CardGrid);
