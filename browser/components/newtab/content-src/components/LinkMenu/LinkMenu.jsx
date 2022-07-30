/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { connect } from "react-redux";
import { ContextMenu } from "content-src/components/ContextMenu/ContextMenu";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";
import React from "react";

const DEFAULT_SITE_MENU_OPTIONS = [
  "CheckPinTopSite",
  "EditTopSite",
  "Separator",
  "OpenInNewWindow",
  "OpenInPrivateWindow",
  "Separator",
  "BlockUrl",
];

export class _LinkMenu extends React.PureComponent {
  getOptions() {
    const { props } = this;
    const {
      site,
      index,
      source,
      isPrivateBrowsingEnabled,
      siteInfo,
      platform,
      userEvent = ac.UserEvent,
    } = props;

    // Handle special case of default site
    const propOptions =
      site.isDefault && !site.searchTopSite && !site.sponsored_position
        ? DEFAULT_SITE_MENU_OPTIONS
        : props.options;

    const options = propOptions
      .map(o =>
        LinkMenuOptions[o](
          site,
          index,
          source,
          isPrivateBrowsingEnabled,
          siteInfo,
          platform
        )
      )
      .map(option => {
        const { action, impression, id, type, userEvent: eventName } = option;
        if (!type && id) {
          option.onClick = (event = {}) => {
            const { ctrlKey, metaKey, shiftKey, button } = event;
            // Only send along event info if there's something non-default to send
            if (ctrlKey || metaKey || shiftKey || button === 1) {
              action.data = Object.assign(
                {
                  event: { ctrlKey, metaKey, shiftKey, button },
                },
                action.data
              );
            }
            props.dispatch(action);
            if (eventName) {
              const userEventData = Object.assign(
                {
                  event: eventName,
                  source,
                  action_position: index,
                },
                siteInfo
              );
              props.dispatch(userEvent(userEventData));
            }
            if (impression && props.shouldSendImpressionStats) {
              props.dispatch(impression);
            }
          };
        }
        return option;
      });

    // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.
    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return (
      <ContextMenu
        onUpdate={this.props.onUpdate}
        onShow={this.props.onShow}
        options={this.getOptions()}
        keyboardAccess={this.props.keyboardAccess}
      />
    );
  }
}

const getState = state => ({
  isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled,
  platform: state.Prefs.values.platform,
});
export const LinkMenu = connect(getState)(_LinkMenu);
