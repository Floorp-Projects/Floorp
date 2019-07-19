/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import React from "react";

export class DSLinkMenu extends React.PureComponent {
  constructor(props) {
    super(props);
    this.windowObj = this.props.windowObj || window; // Added to support unit tests
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
    this.contextMenuButtonRef = React.createRef();
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
      dsLinkMenuHostDiv.parentElement.classList.remove("active", "last-item");
    }
  }

  nextAnimationFrame() {
    return new Promise(resolve => requestAnimationFrame(resolve));
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
    // Wait for next frame before computing scrollMaxX to allow fluent menu strings to be visible
    await this.nextAnimationFrame();
    if (this.windowObj.scrollMaxX > 0) {
      dsLinkMenuHostDiv.parentElement.classList.add("last-item");
    }
    dsLinkMenuHostDiv.parentElement.classList.add("active");
  }

  render() {
    const { index, dispatch } = this.props;
    const TOP_STORIES_CONTEXT_MENU_OPTIONS = [
      "CheckBookmarkOrArchive",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
    ];
    const type = this.props.type || "DISCOVERY_STREAM";
    const title = this.props.title || this.props.source;

    return (
      <div>
        <ContextMenuButton
          refFunction={this.contextMenuButtonRef}
          tooltip={"newtab-menu-content-tooltip"}
          tooltipArgs={{ title }}
          onUpdate={this.onMenuUpdate}
        >
          <LinkMenu
            dispatch={dispatch}
            index={index}
            source={type.toUpperCase()}
            onShow={this.onMenuShow}
            options={TOP_STORIES_CONTEXT_MENU_OPTIONS}
            shouldSendImpressionStats={true}
            site={{
              referrer: "https://getpocket.com/recommendations",
              title: this.props.title,
              type: this.props.type,
              url: this.props.url,
              guid: this.props.id,
              pocket_id: this.props.pocket_id,
              shim: this.props.shim,
              bookmarkGuid: this.props.bookmarkGuid,
            }}
          />
        </ContextMenuButton>
      </div>
    );
  }
}
