/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";

import { getLibraryFromUrl } from "../../../utils/pause/frames";

import AccessibleImage from "../../shared/AccessibleImage";
import FrameComponent from "./Frame";

import "./Group.css";

import Badge from "../../shared/Badge";
import FrameIndent from "./FrameIndent";

const classnames = require("devtools/client/shared/classnames.js");

function FrameLocation({ frame, expanded }) {
  const library = frame.library || getLibraryFromUrl(frame);
  if (!library) {
    return null;
  }

  const arrowClassName = classnames("arrow", { expanded });
  return (
    <span className="group-description">
      <AccessibleImage className={arrowClassName} />
      <AccessibleImage className={`annotation-logo ${library.toLowerCase()}`} />
      <span className="group-description-name">{library}</span>
    </span>
  );
}

FrameLocation.propTypes = {
  expanded: PropTypes.any.isRequired,
  frame: PropTypes.object.isRequired,
};

FrameLocation.displayName = "FrameLocation";

export default class Group extends Component {
  constructor(...args) {
    super(...args);
    this.state = { expanded: false };
  }

  static get propTypes() {
    return {
      disableContextMenu: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      getFrameTitle: PropTypes.func,
      group: PropTypes.array.isRequired,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectLocation: PropTypes.func,
      selectedFrame: PropTypes.object,
    };
  }

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  onContextMenu(event) {
    const { group } = this.props;
    const frame = group[0];
    this.props.showFrameContextMenu(event, frame, true);
  }

  toggleFrames = event => {
    event.stopPropagation();
    this.setState(prevState => ({ expanded: !prevState.expanded }));
  };

  renderFrames() {
    const {
      group,
      selectFrame,
      selectLocation,
      selectedFrame,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      showFrameContextMenu,
    } = this.props;

    const { expanded } = this.state;
    if (!expanded) {
      return null;
    }

    return (
      <div className="frames-list">
        {group.reduce((acc, frame, i) => {
          if (this.isSelectable) {
            acc.push(<FrameIndent key={`frame-indent-${i}`} />);
          }
          return acc.concat(
            <FrameComponent
              frame={frame}
              showFrameContextMenu={showFrameContextMenu}
              hideLocation={true}
              key={frame.id}
              selectedFrame={selectedFrame}
              selectFrame={selectFrame}
              selectLocation={selectLocation}
              shouldMapDisplayName={false}
              displayFullUrl={displayFullUrl}
              getFrameTitle={getFrameTitle}
              disableContextMenu={disableContextMenu}
              panel={panel}
            />
          );
        }, [])}
      </div>
    );
  }

  renderDescription() {
    const { l10n } = this.context;
    const { group } = this.props;
    const { expanded } = this.state;

    const frame = group[0];
    const l10NEntry = expanded
      ? "callStack.group.collapseTooltip"
      : "callStack.group.expandTooltip";
    const title = l10n.getFormatStr(l10NEntry, frame.library);

    return (
      <div
        role="listitem"
        key={frame.id}
        className="group"
        onClick={this.toggleFrames}
        tabIndex={0}
        title={title}
      >
        {this.isSelectable && <FrameIndent />}
        <FrameLocation frame={frame} expanded={expanded} />
        {this.isSelectable && <span className="clipboard-only"> </span>}
        <Badge>{this.props.group.length}</Badge>
        {this.isSelectable && <br className="clipboard-only" />}
      </div>
    );
  }

  render() {
    const { expanded } = this.state;
    const { disableContextMenu } = this.props;
    return (
      <div
        className={classnames("frames-group", { expanded })}
        onContextMenu={disableContextMenu ? null : e => this.onContextMenu(e)}
      >
        {this.renderDescription()}
        {this.renderFrames()}
      </div>
    );
  }
}

Group.displayName = "Group";
Group.contextTypes = { l10n: PropTypes.object };
