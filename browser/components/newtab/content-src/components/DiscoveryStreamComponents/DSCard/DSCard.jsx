/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { DSImage } from "../DSImage/DSImage.jsx";
import { DSLinkMenu } from "../DSLinkMenu/DSLinkMenu";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import {
  DSContextFooter,
  SponsorLabel,
  DSMessageFooter,
} from "../DSContextFooter/DSContextFooter.jsx";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import { connect } from "react-redux";

const READING_WPM = 220;

/**
 * READ TIME FROM WORD COUNT
 * @param {int} wordCount number of words in an article
 * @returns {int} number of words per minute in minutes
 */
export function readTimeFromWordCount(wordCount) {
  if (!wordCount) return false;
  return Math.ceil(parseInt(wordCount, 10) / READING_WPM);
}

export const DSSource = ({
  source,
  timeToRead,
  newSponsoredLabel,
  context,
  sponsor,
  sponsored_by_override,
}) => {
  // First try to display sponsored label or time to read here.
  if (newSponsoredLabel) {
    // If we can display something for spocs, do so.
    if (sponsored_by_override || sponsor || context) {
      return (
        <SponsorLabel
          context={context}
          sponsor={sponsor}
          sponsored_by_override={sponsored_by_override}
          newSponsoredLabel="new-sponsored-label"
        />
      );
    }
  }

  // If we are not a spoc, and can display a time to read value.
  if (timeToRead) {
    return (
      <p className="source clamp time-to-read">
        <FluentOrText
          message={{
            id: `newtab-label-source-read-time`,
            values: { source, timeToRead },
          }}
        />
      </p>
    );
  }

  // Otherwise display a default source.
  return <p className="source clamp">{source}</p>;
};

export const DefaultMeta = ({
  source,
  title,
  excerpt,
  timeToRead,
  newSponsoredLabel,
  context,
  context_type,
  sponsor,
  sponsored_by_override,
  saveToPocketCard,
  isRecentSave,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <DSSource
        source={source}
        timeToRead={timeToRead}
        newSponsoredLabel={newSponsoredLabel}
        context={context}
        sponsor={sponsor}
        sponsored_by_override={sponsored_by_override}
      />
      <header title={title} className="title clamp">
        {title}
      </header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
    </div>
    {!newSponsoredLabel && (
      <DSContextFooter
        context_type={context_type}
        context={context}
        sponsor={sponsor}
        sponsored_by_override={sponsored_by_override}
      />
    )}
    {/* Sponsored label is normally in the way of any message.
        newSponsoredLabel cards sponsored label is moved to just under the thumbnail,
        so we can display both, so we specifically don't pass in context. */}
    {newSponsoredLabel && (
      <DSMessageFooter
        context_type={context_type}
        context={null}
        saveToPocketCard={saveToPocketCard}
      />
    )}
  </div>
);

export class _DSCard extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onLinkClick = this.onLinkClick.bind(this);
    this.onSaveClick = this.onSaveClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);

    this.setContextMenuButtonHostRef = element => {
      this.contextMenuButtonHostElement = element;
    };
    this.setPlaceholderRef = element => {
      this.placeholderElement = element;
    };

    this.state = {
      isSeen: false,
    };

    // If this is for the about:home startup cache, then we always want
    // to render the DSCard, regardless of whether or not its been seen.
    if (props.App.isForStartupCache) {
      this.state.isSeen = true;
    }

    // We want to choose the optimal thumbnail for the underlying DSImage, but
    // want to do it in a performant way. The breakpoints used in the
    // CSS of the page are, unfortuntely, not easy to retrieve without
    // causing a style flush. To avoid that, we hardcode them here.
    //
    // The values chosen here were the dimensions of the card thumbnails as
    // computed by getBoundingClientRect() for each type of viewport width
    // across both high-density and normal-density displays.
    this.dsImageSizes = [
      {
        mediaMatcher: "(min-width: 1122px)",
        width: 296,
        height: 148,
      },

      {
        mediaMatcher: "(min-width: 866px)",
        width: 218,
        height: 109,
      },

      {
        mediaMatcher: "(max-width: 610px)",
        width: 202,
        height: 101,
      },
    ];
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source: this.props.type.toUpperCase(),
          action_position: this.props.pos,
          value: { card_type: this.props.flightId ? "spoc" : "organic" },
        })
      );

      this.props.dispatch(
        ac.ImpressionStats({
          source: this.props.type.toUpperCase(),
          click: 0,
          window_inner_width: this.props.windowObj.innerWidth,
          window_inner_height: this.props.windowObj.innerHeight,
          tiles: [
            {
              id: this.props.id,
              pos: this.props.pos,
              ...(this.props.shim && this.props.shim.click
                ? { shim: this.props.shim.click }
                : {}),
              type: this.props.flightId ? "spoc" : "organic",
            },
          ],
        })
      );
    }
  }

  onSaveClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.AlsoToMain({
          type: at.SAVE_TO_POCKET,
          data: { site: { url: this.props.url, title: this.props.title } },
        })
      );

      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "SAVE_TO_POCKET",
          source: "CARDGRID_HOVER",
          action_position: this.props.pos,
          value: { card_type: this.props.flightId ? "spoc" : "organic" },
        })
      );

      this.props.dispatch(
        ac.ImpressionStats({
          source: "CARDGRID_HOVER",
          pocket: 0,
          tiles: [
            {
              id: this.props.id,
              pos: this.props.pos,
              ...(this.props.shim && this.props.shim.save
                ? { shim: this.props.shim.save }
                : {}),
            },
          ],
        })
      );
    }
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;
      if (dsLinkMenuHostDiv) {
        dsLinkMenuHostDiv.classList.remove("active", "last-item");
      }
    }
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;
    if (dsLinkMenuHostDiv) {
      // Force translation so we can be sure it's ready before measuring.
      await this.props.windowObj.document.l10n.translateFragment(
        dsLinkMenuHostDiv
      );
      if (this.props.windowObj.scrollMaxX > 0) {
        dsLinkMenuHostDiv.classList.add("last-item");
      }
      dsLinkMenuHostDiv.classList.add("active");
    }
  }

  onSeen(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.placeholderElement) {
          this.observer.unobserve(this.placeholderElement);
        }

        // Stop observing since element has been seen
        this.setState({
          isSeen: true,
        });
      }
    }
  }

  onIdleCallback() {
    if (!this.state.isSeen) {
      if (this.observer && this.placeholderElement) {
        this.observer.unobserve(this.placeholderElement);
      }
      this.setState({
        isSeen: true,
      });
    }
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(
      this.onIdleCallback.bind(this)
    );
    if (this.placeholderElement) {
      this.observer = new IntersectionObserver(this.onSeen.bind(this));
      this.observer.observe(this.placeholderElement);
    }
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer && this.placeholderElement) {
      this.observer.unobserve(this.placeholderElement);
    }
    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    if (this.props.placeholder || !this.state.isSeen) {
      return (
        <div className="ds-card placeholder" ref={this.setPlaceholderRef} />
      );
    }

    const { isRecentSave, DiscoveryStream, saveToPocketCard } = this.props;

    const {
      pocketButtonEnabled,
      hideDescriptions,
      compactImages,
      imageGradient,
      newSponsoredLabel,
      titleLines = 3,
      descLines = 3,
      readTime: displayReadTime,
    } = DiscoveryStream;

    const excerpt = !hideDescriptions ? this.props.excerpt : "";

    let timeToRead;
    if (displayReadTime) {
      timeToRead =
        this.props.time_to_read || readTimeFromWordCount(this.props.word_count);
    }

    const compactImagesClassName = compactImages ? `ds-card-compact-image` : ``;
    const imageGradientClassName = imageGradient
      ? `ds-card-image-gradient`
      : ``;
    const titleLinesName = `ds-card-title-lines-${titleLines}`;
    const descLinesClassName = `ds-card-desc-lines-${descLines}`;

    return (
      <div
        className={`ds-card ${compactImagesClassName} ${imageGradientClassName} ${titleLinesName} ${descLinesClassName}`}
        ref={this.setContextMenuButtonHostRef}
      >
        <SafeAnchor
          className="ds-card-link"
          dispatch={this.props.dispatch}
          onLinkClick={!this.props.placeholder ? this.onLinkClick : undefined}
          url={this.props.url}
        >
          <div className="img-wrapper">
            <DSImage
              extraClassNames="img"
              source={this.props.image_src}
              rawSource={this.props.raw_image_src}
              sizes={this.dsImageSizes}
              url={this.props.url}
              title={this.props.title}
            />
          </div>
          <DefaultMeta
            source={this.props.source}
            title={this.props.title}
            excerpt={excerpt}
            newSponsoredLabel={newSponsoredLabel}
            timeToRead={timeToRead}
            context={this.props.context}
            context_type={this.props.context_type}
            sponsor={this.props.sponsor}
            sponsored_by_override={this.props.sponsored_by_override}
            saveToPocketCard={saveToPocketCard}
          />
          <ImpressionStats
            flightId={this.props.flightId}
            rows={[
              {
                id: this.props.id,
                pos: this.props.pos,
                ...(this.props.shim && this.props.shim.impression
                  ? { shim: this.props.shim.impression }
                  : {}),
              },
            ]}
            dispatch={this.props.dispatch}
            source={this.props.type}
          />
        </SafeAnchor>
        {saveToPocketCard && (
          <div className="card-stp-button-hover-background">
            <div className="card-stp-button-position-wrapper">
              <button className="card-stp-button" onClick={this.onSaveClick}>
                {this.props.context_type === "pocket" ? (
                  <>
                    <span className="story-badge-icon icon icon-pocket" />
                    <span data-l10n-id="newtab-pocket-saved" />
                  </>
                ) : (
                  <>
                    <span className="story-badge-icon icon icon-pocket-save" />
                    <span data-l10n-id="newtab-pocket-save" />
                  </>
                )}
              </button>
              <DSLinkMenu
                id={this.props.id}
                index={this.props.pos}
                dispatch={this.props.dispatch}
                url={this.props.url}
                title={this.props.title}
                source={this.props.source}
                type={this.props.type}
                pocket_id={this.props.pocket_id}
                shim={this.props.shim}
                bookmarkGuid={this.props.bookmarkGuid}
                flightId={
                  !this.props.is_collection ? this.props.flightId : undefined
                }
                showPrivacyInfo={!!this.props.flightId}
                onMenuUpdate={this.onMenuUpdate}
                onMenuShow={this.onMenuShow}
                saveToPocketCard={saveToPocketCard}
                pocket_button_enabled={pocketButtonEnabled}
                isRecentSave={isRecentSave}
              />
            </div>
          </div>
        )}
        {!saveToPocketCard && (
          <DSLinkMenu
            id={this.props.id}
            index={this.props.pos}
            dispatch={this.props.dispatch}
            url={this.props.url}
            title={this.props.title}
            source={this.props.source}
            type={this.props.type}
            pocket_id={this.props.pocket_id}
            shim={this.props.shim}
            bookmarkGuid={this.props.bookmarkGuid}
            flightId={
              !this.props.is_collection ? this.props.flightId : undefined
            }
            showPrivacyInfo={!!this.props.flightId}
            hostRef={this.contextMenuButtonHostRef}
            onMenuUpdate={this.onMenuUpdate}
            onMenuShow={this.onMenuShow}
            pocket_button_enabled={pocketButtonEnabled}
            isRecentSave={isRecentSave}
          />
        )}
      </div>
    );
  }
}

_DSCard.defaultProps = {
  windowObj: window, // Added to support unit tests
};

export const DSCard = connect(state => ({
  App: state.App,
  DiscoveryStream: state.DiscoveryStream,
}))(_DSCard);

export const PlaceholderDSCard = props => <DSCard placeholder={true} />;
