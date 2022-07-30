/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";

export class DSSignup extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      active: false,
      lastItem: false,
    };
    this.onMenuButtonUpdate = this.onMenuButtonUpdate.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
  }

  onMenuButtonUpdate(showContextMenu) {
    if (!showContextMenu) {
      this.setState({
        active: false,
        lastItem: false,
      });
    }
  }

  nextAnimationFrame() {
    return new Promise(resolve =>
      this.props.windowObj.requestAnimationFrame(resolve)
    );
  }

  async onMenuShow() {
    let { lastItem } = this.state;
    // Wait for next frame before computing scrollMaxX to allow fluent menu strings to be visible
    await this.nextAnimationFrame();
    if (this.props.windowObj.scrollMaxX > 0) {
      lastItem = true;
    }
    this.setState({
      active: true,
      lastItem,
    });
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

  render() {
    const { data, dispatch, type } = this.props;
    if (!data || !data.spocs || !data.spocs[0]) {
      return null;
    }
    // Grab the first item in the array as we only have 1 spoc position.
    const [spoc] = data.spocs;
    const { title, url, excerpt, flight_id, id, shim } = spoc;

    const SIGNUP_CONTEXT_MENU_OPTIONS = [
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
      ...(flight_id ? ["ShowPrivacyInfo"] : []),
    ];

    const outerClassName = [
      "ds-signup",
      this.state.active && "active",
      this.state.lastItem && "last-item",
    ]
      .filter(v => v)
      .join(" ");

    return (
      <div className={outerClassName}>
        <div className="ds-signup-content">
          <span className="icon icon-small-spacer icon-mail"></span>
          <span>
            {title}{" "}
            <SafeAnchor
              className="ds-chevron-link"
              dispatch={dispatch}
              onLinkClick={this.onLinkClick}
              url={url}
            >
              {excerpt}
            </SafeAnchor>
          </span>
          <ImpressionStats
            flightId={flight_id}
            rows={[
              {
                id,
                pos: 0,
                shim: shim && shim.impression,
              },
            ]}
            dispatch={dispatch}
            source={type}
          />
        </div>
        <ContextMenuButton
          tooltip={"newtab-menu-content-tooltip"}
          tooltipArgs={{ title }}
          onUpdate={this.onMenuButtonUpdate}
        >
          <LinkMenu
            dispatch={dispatch}
            index={0}
            source={type.toUpperCase()}
            onShow={this.onMenuShow}
            options={SIGNUP_CONTEXT_MENU_OPTIONS}
            shouldSendImpressionStats={true}
            userEvent={ac.DiscoveryStreamUserEvent}
            site={{
              referrer: "https://getpocket.com/recommendations",
              title,
              type,
              url,
              guid: id,
              shim,
              flight_id,
            }}
          />
        </ContextMenuButton>
      </div>
    );
  }
}

DSSignup.defaultProps = {
  windowObj: window, // Added to support unit tests
};
