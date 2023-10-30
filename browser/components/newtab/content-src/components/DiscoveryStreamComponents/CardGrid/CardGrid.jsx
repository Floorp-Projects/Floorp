/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DSCard, PlaceholderDSCard } from "../DSCard/DSCard.jsx";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState.jsx";
import { DSDismiss } from "content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss";
import { TopicsWidget } from "../TopicsWidget/TopicsWidget.jsx";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import React, { useEffect, useState, useRef, useCallback } from "react";
import { connect, useSelector } from "react-redux";
const PREF_ONBOARDING_EXPERIENCE_DISMISSED =
  "discoverystream.onboardingExperience.dismissed";
const INTERSECTION_RATIO = 0.5;
const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const WIDGET_IDS = {
  TOPICS: 1,
};

export function DSSubHeader({ children }) {
  return (
    <div className="section-top-bar ds-sub-header">
      <h3 className="section-title-container">{children}</h3>
    </div>
  );
}

export function OnboardingExperience({
  children,
  dispatch,
  windowObj = global,
}) {
  const [dismissed, setDismissed] = useState(false);
  const [maxHeight, setMaxHeight] = useState(null);
  const heightElement = useRef(null);

  const onDismissClick = useCallback(() => {
    // We update this as state and redux.
    // The state update is for this newtab,
    // and the redux update is for other tabs, offscreen tabs, and future tabs.
    // We need the state update for this tab to support the transition.
    setDismissed(true);
    dispatch(ac.SetPref(PREF_ONBOARDING_EXPERIENCE_DISMISSED, true));
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "BLOCK",
        source: "POCKET_ONBOARDING",
      })
    );
  }, [dispatch]);

  useEffect(() => {
    const resizeObserver = new windowObj.ResizeObserver(() => {
      if (heightElement.current) {
        setMaxHeight(heightElement.current.offsetHeight);
      }
    });

    const options = { threshold: INTERSECTION_RATIO };
    const intersectionObserver = new windowObj.IntersectionObserver(entries => {
      if (
        entries.some(
          entry =>
            entry.isIntersecting &&
            entry.intersectionRatio >= INTERSECTION_RATIO
        )
      ) {
        dispatch(
          ac.DiscoveryStreamUserEvent({
            event: "IMPRESSION",
            source: "POCKET_ONBOARDING",
          })
        );
        // Once we have observed an impression, we can stop for this instance of newtab.
        intersectionObserver.unobserve(heightElement.current);
      }
    }, options);

    const onVisibilityChange = () => {
      intersectionObserver.observe(heightElement.current);
      windowObj.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        onVisibilityChange
      );
    };

    if (heightElement.current) {
      resizeObserver.observe(heightElement.current);
      // Check visibility or setup a visibility event to make
      // sure we don't fire this for off screen pre loaded tabs.
      if (windowObj.document.visibilityState === VISIBLE) {
        intersectionObserver.observe(heightElement.current);
      } else {
        windowObj.document.addEventListener(
          VISIBILITY_CHANGE_EVENT,
          onVisibilityChange
        );
      }
      setMaxHeight(heightElement.current.offsetHeight);
    }

    // Return unmount callback to clean up observers.
    return () => {
      resizeObserver?.disconnect();
      intersectionObserver?.disconnect();
      windowObj.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        onVisibilityChange
      );
    };
  }, [dispatch, windowObj]);

  const style = {};
  if (dismissed) {
    style.maxHeight = "0";
    style.opacity = "0";
    style.transition = "max-height 0.26s ease, opacity 0.26s ease";
  } else if (maxHeight) {
    style.maxHeight = `${maxHeight}px`;
  }

  return (
    <div style={style}>
      <div className="ds-onboarding-ref" ref={heightElement}>
        <div className="ds-onboarding-container">
          <DSDismiss
            onDismissClick={onDismissClick}
            extraClasses={`ds-onboarding`}
          >
            <div>
              <header>
                <span className="icon icon-pocket" />
                <span data-l10n-id="newtab-pocket-onboarding-discover" />
              </header>
              <p data-l10n-id="newtab-pocket-onboarding-cta" />
            </div>
            <div className="ds-onboarding-graphic" />
          </DSDismiss>
        </div>
      </div>
    </div>
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
  gridClassName = "",
  dispatch,
  windowObj = window,
  items = 3,
  source = "CARDGRID_RECENT_SAVES",
}) {
  const {
    recentSavesData,
    isUserLoggedIn,
    experimentData: { utmCampaign, utmContent, utmSource },
  } = useSelector(state => state.DiscoveryStream);

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

  let queryParams = `?utm_source=${utmSource}`;
  // We really only need to add these params to urls we own.
  if (utmCampaign && utmContent) {
    queryParams += `&utm_content=${utmContent}&utm_campaign=${utmCampaign}`;
  }

  function renderCard(rec, index) {
    const url = new URL(rec.url);
    const urlSearchParams = new URLSearchParams(queryParams);
    if (rec?.id && !url.href.match(/getpocket\.com\/read/)) {
      url.href = `https://getpocket.com/read/${rec.id}`;
    }

    for (let [key, val] of urlSearchParams.entries()) {
      url.searchParams.set(key, val);
    }

    return (
      <DSCard
        key={`dscard-${rec?.id || index}`}
        id={rec.id}
        pos={index}
        type={source}
        image_src={rec.image_src}
        raw_image_src={rec.raw_image_src}
        word_count={rec.word_count}
        time_to_read={rec.time_to_read}
        title={rec.title}
        excerpt={rec.excerpt}
        url={url.href}
        source={rec.domain}
        isRecentSave={true}
        dispatch={dispatch}
      />
    );
  }

  function onMyListClicked() {
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: `${source}_VIEW_LIST`,
      })
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
            id: recentSave.id,
            image_src: recentSave.top_image_url,
            raw_image_src: recentSave.top_image_url,
            word_count: recentSave.word_count,
            time_to_read: recentSave.time_to_read,
            title: recentSave.resolved_title || recentSave.given_title,
            url: recentSave.resolved_url || recentSave.given_url,
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
    <>
      <DSSubHeader>
        <span className="section-title">
          <FluentOrText message="Recently Saved to your List" />
        </span>
        <SafeAnchor
          onLinkClick={onMyListClicked}
          className="section-sub-link"
          url={`https://getpocket.com/a${queryParams}`}
        >
          <FluentOrText message="View My List" />
        </SafeAnchor>
      </DSSubHeader>
      <div className={`ds-card-grid-recent-saves ${gridClassName}`}>
        {recentSavesCards}
      </div>
    </>
  );
}

export class _CardGrid extends React.PureComponent {
  renderCards() {
    const prefs = this.props.Prefs.values;
    const {
      items,
      hybridLayout,
      hideCardBackground,
      fourCardLayout,
      compactGrid,
      essentialReadsHeader,
      editorsPicksHeader,
      onboardingExperience,
      widgets,
      recentSavesEnabled,
      hideDescriptions,
      DiscoveryStream,
    } = this.props;
    const { saveToPocketCard } = DiscoveryStream;
    const showRecentSaves = prefs.showRecentSaves && recentSavesEnabled;
    const isOnboardingExperienceDismissed =
      prefs[PREF_ONBOARDING_EXPERIENCE_DISMISSED];

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
            publisher={rec.publisher}
            pocket_id={rec.pocket_id}
            context_type={rec.context_type}
            bookmarkGuid={rec.bookmarkGuid}
            is_collection={this.props.is_collection}
            saveToPocketCard={saveToPocketCard}
            recommendation_id={rec.recommendation_id}
          />
        )
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
    if (showRecentSaves || (essentialReadsHeader && editorsPicksHeader)) {
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

    const gridClassName = `ds-card-grid ${hybridLayoutClassName} ${hideCardBackgroundClass} ${fourCardLayoutClass} ${hideDescriptionsClassName} ${compactGridClassName}`;

    return (
      <>
        {!isOnboardingExperienceDismissed && onboardingExperience && (
          <OnboardingExperience dispatch={this.props.dispatch} />
        )}
        {essentialReadsCards?.length > 0 && (
          <div className={gridClassName}>{essentialReadsCards}</div>
        )}
        {showRecentSaves && (
          <RecentSavesContainer
            gridClassName={gridClassName}
            dispatch={this.props.dispatch}
          />
        )}
        {editorsPicksCards?.length > 0 && (
          <>
            <DSSubHeader>
              <span className="section-title">
                <FluentOrText message="Editor’s Picks" />
              </span>
            </DSSubHeader>
            <div className={gridClassName}>{editorsPicksCards}</div>
          </>
        )}
        {cards?.length > 0 && (
          <>
            {moreRecsHeader && (
              <DSSubHeader>
                <span className="section-title">
                  <FluentOrText message={moreRecsHeader} />
                </span>
              </DSSubHeader>
            )}
            <div className={gridClassName}>{cards}</div>
          </>
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
      </div>
    );
  }
}

_CardGrid.defaultProps = {
  items: 4, // Number of stories to display
};

export const CardGrid = connect(state => ({
  Prefs: state.Prefs,
  DiscoveryStream: state.DiscoveryStream,
}))(_CardGrid);
