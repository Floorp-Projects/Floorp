/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { isEqual } from "lodash";

import { connect } from "../../utils/connect";
import {
  getFramePositions,
  getSelectedFrame,
  getThreadContext,
} from "../../selectors";
import type { SourceLocation, Frame } from "../../types";

import { getSelectedLocation } from "../../reducers/sources";

import actions from "../../actions";

import classnames from "classnames";
import "./FrameTimeline.css";

type Props = {
  framePositions: any,
  selectedLocation: ?SourceLocation,
  previewLocation: typeof actions.previewPausedLocation,
  seekToPosition: typeof actions.seekToPosition,
  selectedFrame: Frame,
};
type OwnProps = {};

type State = {
  scrubbing: boolean,
  percentage: number,
  displayedLocation: any,
  displayedFrame: Frame,
};

function isSameLocation(
  frameLocation: SourceLocation,
  selectedLocation: ?SourceLocation
) {
  if (!frameLocation.sourceUrl || !selectedLocation) {
    return;
  }

  return (
    frameLocation.line === selectedLocation.line &&
    frameLocation.column === selectedLocation.column &&
    selectedLocation.sourceId.includes(frameLocation.sourceUrl)
  );
}

function getBoundingClientRect(element: ?HTMLElement) {
  if (!element) {
    // $FlowIgnore
    return;
  }
  return element.getBoundingClientRect();
}

class FrameTimeline extends Component<Props, State> {
  _timeline: ?HTMLElement;
  _marker: ?HTMLElement;

  constructor(props: Props) {
    super(props);
  }

  state = {
    scrubbing: false,
    percentage: 0,
    displayedLocation: null,
    displayedFrame: {},
  };

  componentDidUpdate(prevProps: Props, prevState: State) {
    if (!document.body) {
      return;
    }

    // To please Flow.
    const bodyClassList = document.body.classList;

    if (this.state.scrubbing && !prevState.scrubbing) {
      document.addEventListener("mousemove", this.onMouseMove);
      document.addEventListener("mouseup", this.onMouseUp);
      bodyClassList.add("scrubbing");
    }
    if (!this.state.scrubbing && prevState.scrubbing) {
      document.removeEventListener("mousemove", this.onMouseMove);
      document.removeEventListener("mouseup", this.onMouseUp);
      bodyClassList.remove("scrubbing");
    }
  }

  getProgress(clientX: number) {
    const { width, left } = getBoundingClientRect(this._timeline);
    const progress = ((clientX - left) / width) * 100;

    if (progress < 0) {
      return 0;
    } else if (progress > 99) {
      return 99;
    }

    return progress;
  }

  getPosition(percentage: ?number) {
    const { framePositions } = this.props;
    if (!framePositions) {
      return;
    }

    if (!percentage) {
      percentage = this.state.percentage;
    }

    const displayedPositions = framePositions.filter(
      point => point.position.kind === "OnStep"
    );
    const displayIndex = Math.floor(
      (percentage / 100) * displayedPositions.length
    );

    return displayedPositions[displayIndex];
  }

  displayPreview(percentage: number) {
    const { previewLocation } = this.props;

    const position = this.getPosition(percentage);

    if (position) {
      previewLocation(position.location);
    }
  }

  onMouseDown = (event: SyntheticMouseEvent<>) => {
    const progress = this.getProgress(event.clientX);
    this.setState({ scrubbing: true, percentage: progress });
  };

  onMouseUp = (event: MouseEvent) => {
    const { seekToPosition, selectedLocation } = this.props;

    const progress = this.getProgress(event.clientX);
    const position = this.getPosition(progress);
    this.setState({
      scrubbing: false,
      percentage: progress,
      displayedLocation: selectedLocation,
    });

    if (position) {
      seekToPosition(position);
    }
  };

  onMouseMove = (event: MouseEvent) => {
    const percentage = this.getProgress(event.clientX);

    this.displayPreview(percentage);
    this.setState({ percentage });
  };

  getProgressForNewFrame() {
    const { framePositions, selectedLocation, selectedFrame } = this.props;
    this.setState({
      displayedLocation: selectedLocation,
      displayedFrame: selectedFrame,
    });
    let progress = 0;

    if (!framePositions) {
      return progress;
    }

    const displayedPositions = framePositions.filter(
      point => point.position.kind === "OnStep"
    );
    const index = displayedPositions.findIndex(pos =>
      isSameLocation(pos.location, selectedLocation)
    );

    if (index != -1) {
      progress = Math.floor((index / displayedPositions.length) * 100);
      this.setState({ percentage: progress });
    }

    return progress;
  }

  getVisibleProgress() {
    const {
      percentage,
      displayedLocation,
      displayedFrame,
      scrubbing,
    } = this.state;
    const { selectedLocation, selectedFrame } = this.props;

    let progress = percentage;

    if (
      !isEqual(displayedLocation, selectedLocation) &&
      displayedFrame.index !== selectedFrame.index &&
      !scrubbing
    ) {
      progress = this.getProgressForNewFrame();
    }

    return progress;
  }

  renderMarker() {
    return (
      <div className="frame-timeline-marker" ref={r => (this._marker = r)} />
    );
  }

  renderProgress() {
    const progress = this.getVisibleProgress();
    let maxWidth = "100%";
    if (this._timeline && this._marker) {
      const timelineWidth = getBoundingClientRect(this._timeline).width;
      const markerWidth = getBoundingClientRect(this._timeline).width;
      maxWidth = timelineWidth - markerWidth - 2;
    }

    return (
      <div
        className="frame-timeline-progress"
        style={{
          width: `${progress}%`,
          "max-width": maxWidth,
        }}
      />
    );
  }

  renderTimeline() {
    return (
      <div
        className="frame-timeline-bar"
        onMouseDown={this.onMouseDown}
        ref={r => (this._timeline = r)}
      >
        {this.renderProgress()}
        {this.renderMarker()}
      </div>
    );
  }

  render() {
    const { scrubbing } = this.state;
    const { framePositions } = this.props;

    if (!framePositions) {
      return null;
    }

    return (
      <div className={classnames("frame-timeline-container", { scrubbing })}>
        {this.renderTimeline()}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const selectedFrame: Frame = (getSelectedFrame(
    state,
    getThreadContext(state).thread
  ): any);

  return {
    framePositions: getFramePositions(state),
    selectedLocation: getSelectedLocation(state),
    selectedFrame,
  };
};

export default connect<Props, OwnProps, _, _, _, _>(
  mapStateToProps,
  {
    seekToPosition: actions.seekToPosition,
    previewLocation: actions.previewPausedLocation,
  }
)(FrameTimeline);
