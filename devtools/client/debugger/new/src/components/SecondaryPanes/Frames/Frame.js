/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import classNames from "classnames";
import Svg from "../../shared/Svg";

import { formatDisplayName } from "../../../utils/pause/frames";
import { getFilename, getFileURL } from "../../../utils/source";
import FrameMenu from "./FrameMenu";

import type { Frame } from "../../../types";
import type { LocalFrame } from "./types";

type FrameTitleProps = {
  frame: Frame,
  options: Object
};

function FrameTitle({ frame, options = {} }: FrameTitleProps) {
  const displayName = formatDisplayName(frame, { ...options, maxLength: null });
  return <div className="title">{displayName}</div>;
}

type FrameLocationProps = { frame: LocalFrame, displayFullUrl: boolean };

function FrameLocation({ frame, displayFullUrl = false }: FrameLocationProps) {
  if (!frame.source) {
    return null;
  }

  if (frame.library) {
    return (
      <div className="location">
        {frame.library}
        <Svg name={frame.library.toLowerCase()} className="annotation-logo" />
      </div>
    );
  }

  const { location, source } = frame;
  const filename = displayFullUrl
    ? getFileURL(source, false)
    : getFilename(source);

  return <div className="location">{`${filename}:${location.line}`}</div>;
}

FrameLocation.displayName = "FrameLocation";

type FrameComponentProps = {
  frame: LocalFrame,
  selectedFrame: LocalFrame,
  copyStackTrace: Function,
  toggleFrameworkGrouping: Function,
  selectFrame: Function,
  frameworkGroupingOn: boolean,
  hideLocation: boolean,
  shouldMapDisplayName: boolean,
  toggleBlackBox: Function,
  displayFullUrl: boolean,
  getFrameTitle?: string => string
};

export default class FrameComponent extends Component<FrameComponentProps> {
  static defaultProps = {
    hideLocation: false,
    shouldMapDisplayName: true
  };

  onContextMenu(event: SyntheticKeyboardEvent<HTMLElement>) {
    const {
      frame,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn
    } = this.props;
    FrameMenu(
      frame,
      frameworkGroupingOn,
      { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox },
      event
    );
  }

  onMouseDown(
    e: SyntheticKeyboardEvent<HTMLElement>,
    frame: Frame,
    selectedFrame: Frame
  ) {
    if (e.which == 3) {
      return;
    }
    this.props.selectFrame(frame);
  }

  onKeyUp(
    event: SyntheticKeyboardEvent<HTMLElement>,
    frame: Frame,
    selectedFrame: Frame
  ) {
    if (event.key != "Enter") {
      return;
    }
    this.props.selectFrame(frame);
  }

  render() {
    const {
      frame,
      selectedFrame,
      hideLocation,
      shouldMapDisplayName,
      displayFullUrl,
      getFrameTitle
    } = this.props;

    const className = classNames("frame", {
      selected: selectedFrame && selectedFrame.id === frame.id
    });

    const title = getFrameTitle
      ? getFrameTitle(
          `${getFileURL(frame.source, false)}:${frame.location.line}`
        )
      : undefined;

    return (
      <li
        key={frame.id}
        className={className}
        onMouseDown={e => this.onMouseDown(e, frame, selectedFrame)}
        onKeyUp={e => this.onKeyUp(e, frame, selectedFrame)}
        onContextMenu={e => this.onContextMenu(e)}
        tabIndex={0}
        title={title}
      >
        <FrameTitle frame={frame} options={{ shouldMapDisplayName }} />
        {!hideLocation && (
          <FrameLocation frame={frame} displayFullUrl={displayFullUrl} />
        )}
      </li>
    );
  }
}

FrameComponent.displayName = "Frame";
