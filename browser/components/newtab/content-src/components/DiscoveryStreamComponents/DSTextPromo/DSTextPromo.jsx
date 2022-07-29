/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { DSDismiss } from "content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss";
import { DSImage } from "../DSImage/DSImage.jsx";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";

export class DSTextPromo extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onDismissClick = this.onDismissClick.bind(this);
  }

  onLinkClick() {
    const { data } = this.props;
    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      const source = this.props.type.toUpperCase();
      // Grab the first item in the array as we only have 1 spoc position.
      const [spoc] = data.spocs;
      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source,
          action_position: 0,
        })
      );

      this.props.dispatch(
        ac.ImpressionStats({
          source,
          click: 0,
          tiles: [
            {
              id: spoc.id,
              pos: 0,
              ...(spoc.shim && spoc.shim.click
                ? { shim: spoc.shim.click }
                : {}),
            },
          ],
        })
      );
    }
  }

  onDismissClick() {
    const { data } = this.props;
    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      const index = 0;
      const source = this.props.type.toUpperCase();
      // Grab the first item in the array as we only have 1 spoc position.
      const [spoc] = data.spocs;
      const spocData = {
        url: spoc.url,
        guid: spoc.id,
        shim: spoc.shim,
      };
      const blockUrlOption = LinkMenuOptions.BlockUrl(spocData, index, source);

      const { action, impression, userEvent } = blockUrlOption;

      this.props.dispatch(action);
      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: userEvent,
          source,
          action_position: index,
        })
      );
      if (impression) {
        this.props.dispatch(impression);
      }
    }
  }

  render() {
    const { data } = this.props;
    if (!data || !data.spocs || !data.spocs[0]) {
      return null;
    }
    // Grab the first item in the array as we only have 1 spoc position.
    const [spoc] = data.spocs;
    const {
      image_src,
      raw_image_src,
      alt_text,
      title,
      url,
      context,
      cta,
      flight_id,
      id,
      shim,
    } = spoc;

    return (
      <DSDismiss
        onDismissClick={this.onDismissClick}
        extraClasses={`ds-dismiss-ds-text-promo`}
      >
        <div className="ds-text-promo">
          <DSImage
            alt_text={alt_text}
            source={image_src}
            rawSource={raw_image_src}
          />
          <div className="text">
            <h3>
              {`${title}\u2003`}
              <SafeAnchor
                className="ds-chevron-link"
                dispatch={this.props.dispatch}
                onLinkClick={this.onLinkClick}
                url={url}
              >
                {cta}
              </SafeAnchor>
            </h3>
            <p className="subtitle">{context}</p>
          </div>
          <ImpressionStats
            flightId={flight_id}
            rows={[
              {
                id,
                pos: 0,
                shim: shim && shim.impression,
              },
            ]}
            dispatch={this.props.dispatch}
            source={this.props.type}
          />
        </div>
      </DSDismiss>
    );
  }
}
