/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { DSImage } from "../DSImage/DSImage.jsx";
import { DSLinkMenu } from "../DSLinkMenu/DSLinkMenu";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { DSContextFooter } from "../DSContextFooter/DSContextFooter.jsx";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import { connect } from "react-redux";

// Default Meta that displays CTA as link if cta_variant in layout is set as "link"
export const DefaultMeta = ({
  display_engagement_labels,
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
  engagement,
  cta_variant,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <p className="source clamp">{source}</p>
      <header className="title clamp">{title}</header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
      {cta_variant === "link" && cta && (
        <div role="link" className="cta-link icon icon-arrow" tabIndex="0">
          {cta}
        </div>
      )}
    </div>
    <DSContextFooter
      context_type={context_type}
      context={context}
      display_engagement_labels={display_engagement_labels}
      engagement={engagement}
    />
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
      <header className="title clamp">{title}</header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
    </div>
    {context && cta && <button className="button cta-button">{cta}</button>}
    {!context && (
      <DSContextFooter
        context_type={context_type}
        context={context}
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
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.UserEvent({
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
    const isButtonCTA = this.props.cta_variant === "button";

    return (
      <div className="ds-card">
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
            />
          </div>
          {isButtonCTA ? (
            <CTAButtonMeta
              display_engagement_labels={this.props.display_engagement_labels}
              source={this.props.source}
              title={this.props.title}
              excerpt={this.props.excerpt}
              context={this.props.context}
              context_type={this.props.context_type}
              engagement={this.props.engagement}
              cta={this.props.cta}
              sponsor={this.props.sponsor}
            />
          ) : (
            <DefaultMeta
              display_engagement_labels={this.props.display_engagement_labels}
              source={this.props.source}
              title={this.props.title}
              excerpt={this.props.excerpt}
              context={this.props.context}
              engagement={this.props.engagement}
              context_type={this.props.context_type}
              cta={this.props.cta}
              cta_variant={this.props.cta_variant}
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
            source={this.props.type}
          />
        </SafeAnchor>
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
          flightId={this.props.flightId}
        />
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
