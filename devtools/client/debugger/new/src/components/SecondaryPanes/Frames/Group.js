/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import classNames from "classnames";
import Svg from "../../shared/Svg";
import {
  getLibraryFromUrl,
  formatDisplayName
} from "../../../utils/pause/frames";
import FrameMenu from "./FrameMenu";

import "./Group.css";

import FrameComponent from "./Frame";

import type { LocalFrame } from "./types";
import Badge from "../../shared/Badge";

type FrameLocationProps = { frame: LocalFrame };
function FrameLocation({ frame }: FrameLocationProps) {
  const library = frame.library || getLibraryFromUrl(frame);
  if (!library) {
    return null;
  }

  return (
    <div className="location">
      {library}
      <Svg name={library.toLowerCase()} className="annotation-logo" />
    </div>
  );
}

FrameLocation.displayName = "FrameLocation";

type Props = {
  group: LocalFrame[],
  selectedFrame: LocalFrame,
  selectFrame: Function,
  toggleFrameworkGrouping: Function,
  copyStackTrace: Function,
  toggleBlackBox: Function,
  frameworkGroupingOn: boolean,
  displayFullUrl: boolean,
  getFrameTitle?: string => string,
  disableContextMenu: boolean
};

type State = {
  expanded: boolean
};

export default class Group extends Component<Props, State> {
  toggleFrames: Function;

  constructor(...args: any[]) {
    super(...args);
    this.state = { expanded: false };
  }

  onContextMenu(event: SyntheticMouseEvent<HTMLElement>) {
    const {
      group,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn
    } = this.props;
    const frame = group[0];
    FrameMenu(
      frame,
      frameworkGroupingOn,
      { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox },
      event
    );
  }

  toggleFrames = () => {
    this.setState(prevState => ({ expanded: !prevState.expanded }));
  };

  renderFrames() {
    const {
      group,
      selectFrame,
      selectedFrame,
      toggleFrameworkGrouping,
      frameworkGroupingOn,
      toggleBlackBox,
      copyStackTrace,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu
    } = this.props;

    const { expanded } = this.state;
    if (!expanded) {
      return null;
    }

    return (
      <div className="frames-list">
        {group.map(frame => (
          <FrameComponent
            copyStackTrace={copyStackTrace}
            frame={frame}
            frameworkGroupingOn={frameworkGroupingOn}
            hideLocation={true}
            key={frame.id}
            selectedFrame={selectedFrame}
            selectFrame={selectFrame}
            shouldMapDisplayName={false}
            toggleBlackBox={toggleBlackBox}
            toggleFrameworkGrouping={toggleFrameworkGrouping}
            displayFullUrl={displayFullUrl}
            getFrameTitle={getFrameTitle}
            disableContextMenu={disableContextMenu}
          />
        ))}
      </div>
    );
  }

  renderDescription() {
    const frame = this.props.group[0];
    const displayName = formatDisplayName(frame);
    return (
      <li
        key={frame.id}
        className={classNames("group")}
        onClick={this.toggleFrames}
        tabIndex={0}
      >
        <div className="d-flex align-items-center min-width-0">
          <div className="title">{displayName}</div>
          <Badge>{this.props.group.length}</Badge>
        </div>
        <FrameLocation frame={frame} />
      </li>
    );
  }

  render() {
    const { expanded } = this.state;
    const { disableContextMenu } = this.props;
    return (
      <div
        className={classNames("frames-group", { expanded })}
        onContextMenu={disableContextMenu ? null : e => this.onContextMenu(e)}
      >
        {this.renderDescription()}
        {this.renderFrames()}
      </div>
    );
  }
}

Group.displayName = "Group";
