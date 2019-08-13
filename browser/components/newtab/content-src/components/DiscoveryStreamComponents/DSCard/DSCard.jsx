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

export const DefaultMeta = ({
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <p className="source clamp">{source}</p>
      <header className="title clamp">{title}</header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
      {cta && (
        <div role="link" className="cta-link icon icon-arrow" tabIndex="0">
          {cta}
        </div>
      )}
    </div>
    <DSContextFooter context_type={context_type} context={context} />
  </div>
);

export const VariantMeta = ({
  source,
  title,
  excerpt,
  context,
  context_type,
  cta,
  sponsor,
}) => (
  <div className="meta">
    <div className="info-wrap">
      <p className="source clamp">
        {sponsor ? sponsor : source}
        {context && ` · Sponsored`}
      </p>
      <header className="title clamp">{title}</header>
      {excerpt && <p className="excerpt clamp">{excerpt}</p>}
    </div>
    {cta && <button className="button cta-button">{cta}</button>}
    {!context && (
      <DSContextFooter context_type={context_type} context={context} />
    )}
  </div>
);

export class DSCard extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.UserEvent({
          event: "CLICK",
          source: this.props.type.toUpperCase(),
          action_position: this.props.pos,
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

  render() {
    return (
      <div className={`ds-card${this.props.placeholder ? " placeholder" : ""}`}>
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
          {this.props.cta_variant && (
            <VariantMeta
              source={this.props.source}
              title={this.props.title}
              excerpt={this.props.excerpt}
              context={this.props.context}
              context_type={this.props.context_type}
              cta={this.props.cta}
              sponsor={this.props.sponsor}
            />
          )}
          {!this.props.cta_variant && (
            <DefaultMeta
              source={this.props.source}
              title={this.props.title}
              excerpt={this.props.excerpt}
              context={this.props.context}
              context_type={this.props.context_type}
              cta={this.props.cta}
            />
          )}
          <ImpressionStats
            campaignId={this.props.campaignId}
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
        {!this.props.placeholder && (
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
          />
        )}
      </div>
    );
  }
}
export const PlaceholderDSCard = props => <DSCard placeholder={true} />;
