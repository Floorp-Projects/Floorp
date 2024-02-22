/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { getLibraryFromUrl } from "../../../utils/pause/frames/index";

import AccessibleImage from "../../shared/AccessibleImage";
import FrameComponent from "./Frame";
import Badge from "../../shared/Badge";
import FrameIndent from "./FrameIndent";

const classnames = require("resource://devtools/client/shared/classnames.js");

function FrameLocation({ frame, expanded }) {
  const library = frame.library || getLibraryFromUrl(frame);
  if (!library) {
    return null;
  }
  const arrowClassName = classnames("arrow", {
    expanded,
  });
  return React.createElement(
    "span",
    {
      className: "group-description",
    },
    React.createElement(AccessibleImage, {
      className: arrowClassName,
    }),
    React.createElement(AccessibleImage, {
      className: `annotation-logo ${library.toLowerCase()}`,
    }),
    React.createElement(
      "span",
      {
        className: "group-description-name",
      },
      library
    )
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
      showFrameContextMenu: PropTypes.func.isRequired,
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

    return React.createElement(
      "div",
      {
        className: "frames-list",
      },
      group.map(frame =>
        React.createElement(FrameComponent, {
          frame,
          showFrameContextMenu,
          hideLocation: true,
          key: frame.id,
          selectedFrame,
          selectFrame,
          selectLocation,
          shouldMapDisplayName: false,
          displayFullUrl,
          getFrameTitle,
          disableContextMenu,
          panel,
          isInGroup: true,
        })
      )
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

    return React.createElement(
      "div",
      {
        role: "listitem",
        key: frame.id,
        className: "group",
        onClick: this.toggleFrames,
        tabIndex: 0,
        title,
      },
      this.isSelectable && React.createElement(FrameIndent, null),
      React.createElement(FrameLocation, {
        frame,
        expanded,
      }),
      this.isSelectable &&
        React.createElement(
          "span",
          {
            className: "clipboard-only",
          },
          " "
        ),
      React.createElement(Badge, { badgeText: this.props.group.length }),
      this.isSelectable &&
        React.createElement("br", {
          className: "clipboard-only",
        })
    );
  }

  render() {
    const { expanded } = this.state;
    const { disableContextMenu } = this.props;
    return React.createElement(
      "div",
      {
        className: classnames("frames-group", {
          expanded,
        }),
        onContextMenu: disableContextMenu ? null : e => this.onContextMenu(e),
      },
      this.renderDescription(),
      this.renderFrames()
    );
  }
}

Group.displayName = "Group";
Group.contextTypes = { l10n: PropTypes.object };
