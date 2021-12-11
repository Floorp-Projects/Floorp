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
  compact,
  context,
  sponsor,
  sponsored_by_override,
}) => {
  // If we are compact, try to display sponsored label or time to read here.
  if (compact) {
    // If we can display something for spocs, do so.
    if (sponsored_by_override || sponsor || context) {
      return (
        <SponsorLabel
          context={context}
          sponsor={sponsor}
          sponsored_by_override={sponsored_by_override}
        />
      );
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
  }

  // Otherwise display a default source.
  return <p className="source clamp">{source}</p>;
};

// Default Meta that displays CTA as link if cta_variant in layout is set as "link"
export const DefaultMeta = ({
  display_engagement_labels,
  source,
  title,
  excerpt,
  timeToRead,
  compact,
  context,
  context_type,
  cta,
  engagement,
  cta_variant,
  sponsor,
  sponsored_by_override,
  saveToPocketCard,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <DSSource
        source={source}
        compact={compact}
        timeToRead={timeToRead}
        context={context}
        sponsor={sponsor}
        sponsored_by_override={sponsored_by_override}
      />
      <header title={title} className="title clamp">
        {title}
      </header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
      {cta_variant === "link" && cta && (
        <div role="link" className="cta-link icon icon-arrow" tabIndex="0">
          {cta}
        </div>
      )}
    </div>
    {!compact && (
      <DSContextFooter
        context_type={context_type}
        context={context}
        sponsor={sponsor}
        sponsored_by_override={sponsored_by_override}
        display_engagement_labels={display_engagement_labels}
        engagement={engagement}
      />
    )}
    {/* Sponsored label is normally in the way of any message.
        Compact cards sponsored label is moved to just under the thumbnail,
        so we can display both, so we specifically don't pass in context. */}
    {compact && (
      <div className="story-footer">
        <DSMessageFooter
          context_type={context_type}
          context={null}
          display_engagement_labels={display_engagement_labels}
          engagement={engagement}
          saveToPocketCard={saveToPocketCard}
        />
      </div>
    )}
  </div>
);

export const CTAButtonMeta = ({
  display_engagement_labels,
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
  engagement,
  sponsor,
  sponsored_by_override,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <p className="source clamp">
        {context && (
          <FluentOrText
            message={{
              id: `newtab-label-sponsored`,
              values: { sponsorOrSource: sponsor ? sponsor : source },
            }}
          />
        )}

        {!context && (sponsor ? sponsor : source)}
      </p>
      <header title={title} className="title clamp">
        {title}
      </header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
    </div>
    {context && cta && <button className="button cta-button">{cta}</button>}
    {!context && (
      <DSContextFooter
        context_type={context_type}
        context={context}
        sponsor={sponsor}
        sponsored_by_override={sponsored_by_override}
        display_engagement_labels={display_engagement_labels}
        engagement={engagement}
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
        ac.UserEvent({
          event: "CLICK",
          source: this.props.is_video
            ? "CARDGRID_VIDEO"
            : this.props.type.toUpperCase(),
          action_position: this.props.pos,
          value: { card_type: this.props.flightId ? "spoc" : "organic" },
        })
      );

      this.props.dispatch(
        ac.ImpressionStats({
          source: this.props.is_video
            ? "CARDGRID_VIDEO"
            : this.props.type.toUpperCase(),
          click: 0,
          tiles: [
            {
              id: this.props.id,
              pos: this.props.pos,
              ...(this.props.shim && this.props.shim.click
                ? { shim: this.props.shim.click }
                : {}),
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
        ac.UserEvent({
          event: "SAVE_TO_POCKET",
          source: "CARDGRID_HOVER",
          action_position: this.props.pos,
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

    if (this.props.lastCard) {
      return (
        <div className="ds-card last-card-message">
          <div className="img-wrapper">
            <picture className="ds-image img loaded">
              <img
                data-l10n-id="newtab-pocket-last-card-image"
                className="last-card-message-image"
                src="chrome://activity-stream/content/data/content/assets/caught-up-illustration.svg"
                alt="You’re all caught up"
              />
            </picture>
          </div>
          <div className="meta">
            <div className="info-wrap">
              <header
                className="title clamp"
                data-l10n-id="newtab-pocket-last-card-title"
              />
              <p
                className="ds-last-card-desc"
                data-l10n-id="newtab-pocket-last-card-desc"
              />
            </div>
          </div>
        </div>
      );
    }
    const isButtonCTA = this.props.cta_variant === "button";
    const includeDescriptions = this.props.include_descriptions;
    const { saveToPocketCard } = this.props;
    const baseClass = `ds-card ${this.props.is_video ? `video-card` : ``}`;
    const excerpt = includeDescriptions ? this.props.excerpt : "";

    const timeToRead =
      this.props.time_to_read || readTimeFromWordCount(this.props.word_count);

    return (
      <div className={baseClass} ref={this.setContextMenuButtonHostRef}>
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
            />
            {this.props.is_video && (
              <div className="playhead">
                <span>Video Content</span>
              </div>
            )}
          </div>
          {isButtonCTA ? (
            <CTAButtonMeta
              display_engagement_labels={this.props.display_engagement_labels}
              source={this.props.source}
              title={this.props.title}
              excerpt={excerpt}
              timeToRead={timeToRead}
              context={this.props.context}
              context_type={this.props.context_type}
              compact={this.props.compact}
              engagement={this.props.engagement}
              cta={this.props.cta}
              sponsor={this.props.sponsor}
              sponsored_by_override={this.props.sponsored_by_override}
            />
          ) : (
            <DefaultMeta
              display_engagement_labels={this.props.display_engagement_labels}
              source={this.props.source}
              title={this.props.title}
              excerpt={excerpt}
              timeToRead={timeToRead}
              context={this.props.context}
              engagement={this.props.engagement}
              context_type={this.props.context_type}
              compact={this.props.compact}
              cta={this.props.cta}
              cta_variant={this.props.cta_variant}
              sponsor={this.props.sponsor}
              sponsored_by_override={this.props.sponsored_by_override}
              saveToPocketCard={saveToPocketCard}
            />
          )}
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
            source={this.props.is_video ? "CARDGRID_VIDEO" : this.props.type}
          />
        </SafeAnchor>
        {saveToPocketCard && (
          <div className="card-stp-button-hover-background">
            <div className="card-stp-button-position-wrapper">
              <button className="card-stp-button" onClick={this.onSaveClick}>
                {this.props.context_type === "pocket" ? (
                  <>
                    <span className="story-badge-icon icon icon-pocket" />
                    <span data-l10n-id="newtab-pocket-saved-to-pocket" />
                  </>
                ) : (
                  <>
                    <span className="story-badge-icon icon icon-pocket-save" />
                    <span data-l10n-id="newtab-pocket-save-to-pocket" />
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
}))(_DSCard);

export const PlaceholderDSCard = props => <DSCard placeholder={true} />;
export const LastCardMessage = props => <DSCard lastCard={true} />;
