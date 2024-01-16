/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { div, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import SourceIcon from "../shared/SourceIcon";
import { CloseButton } from "../shared/Button";

import actions from "../../actions";

import {
  getDisplayPath,
  getFileURL,
  getSourceQueryString,
  getTruncatedFileName,
  isPretty,
} from "../../utils/source";
import { createLocation } from "../../utils/location";

import {
  getSelectedLocation,
  getSourcesForTabs,
  isSourceBlackBoxed,
} from "../../selectors";

const classnames = require("devtools/client/shared/classnames.js");

class Tab extends PureComponent {
  static get propTypes() {
    return {
      closeTab: PropTypes.func.isRequired,
      onDragEnd: PropTypes.func.isRequired,
      onDragOver: PropTypes.func.isRequired,
      onDragStart: PropTypes.func.isRequired,
      selectSource: PropTypes.func.isRequired,
      source: PropTypes.object.isRequired,
      sourceActor: PropTypes.object.isRequired,
      tabSources: PropTypes.array.isRequired,
      isBlackBoxed: PropTypes.bool.isRequired,
    };
  }

  onContextMenu = event => {
    event.preventDefault();
    this.props.showTabContextMenu(event, this.props.source);
  };

  isSourceSearchEnabled() {
    return this.props.activeSearch === "source";
  }

  render() {
    const {
      selectSource,
      closeTab,
      source,
      sourceActor,
      tabSources,
      onDragOver,
      onDragStart,
      onDragEnd,
      index,
      isActive,
    } = this.props;
    const sourceId = source.id;
    const isPrettyCode = isPretty(source);

    function onClickClose(e) {
      e.stopPropagation();
      closeTab(source);
    }

    function handleTabClick(e) {
      e.preventDefault();
      e.stopPropagation();
      return selectSource(source, sourceActor);
    }

    const className = classnames("source-tab", {
      active: isActive,
      pretty: isPrettyCode,
      blackboxed: this.props.isBlackBoxed,
    });

    const path = getDisplayPath(source, tabSources);
    const query = getSourceQueryString(source);
    return div(
      {
        draggable: true,
        onDragOver: onDragOver,
        onDragStart: onDragStart,
        onDragEnd: onDragEnd,
        className: className,
        "data-index": index,
        "data-source-id": sourceId,
        onClick: handleTabClick,
        // Accommodate middle click to close tab
        onMouseUp: e => e.button === 1 && closeTab(source),
        onContextMenu: this.onContextMenu,
        title: getFileURL(source, false),
      },
      React.createElement(SourceIcon, {
        location: createLocation({
          source,
          sourceActor,
        }),
        forTab: true,
        modifier: icon => (["file", "javascript"].includes(icon) ? null : icon),
      }),
      div(
        {
          className: "filename",
        },
        getTruncatedFileName(source, query),
        path && span(null, `../${path}/..`)
      ),
      React.createElement(CloseButton, {
        handleClick: onClickClose,
        tooltip: L10N.getStr("sourceTabs.closeTabButtonTooltip"),
      })
    );
  }
}

const mapStateToProps = (state, { source }) => {
  return {
    tabSources: getSourcesForTabs(state),
    isBlackBoxed: isSourceBlackBoxed(state, source),
    isActive: source.id === getSelectedLocation(state)?.source.id,
  };
};

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    closeTab: actions.closeTab,
    showTabContextMenu: actions.showTabContextMenu,
  },
  null,
  {
    withRef: true,
  }
)(Tab);
