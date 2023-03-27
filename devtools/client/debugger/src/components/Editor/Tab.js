/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import { showMenu, buildMenu } from "../../context-menu/menu";

import SourceIcon from "../shared/SourceIcon";
import { CloseButton } from "../shared/Button";
import { copyToTheClipboard } from "../../utils/clipboard";

import actions from "../../actions";

import {
  getDisplayPath,
  getFileURL,
  getRawSourceURL,
  getSourceQueryString,
  getTruncatedFileName,
  isPretty,
  shouldBlackbox,
} from "../../utils/source";
import { getTabMenuItems } from "../../utils/tabs";

import {
  getSelectedLocation,
  getActiveSearch,
  getSourcesForTabs,
  isSourceBlackBoxed,
  getContext,
} from "../../selectors";

import classnames from "classnames";

class Tab extends PureComponent {
  static get propTypes() {
    return {
      activeSearch: PropTypes.string,
      closeTab: PropTypes.func.isRequired,
      closeTabs: PropTypes.func.isRequired,
      copyToClipboard: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      onDragEnd: PropTypes.func.isRequired,
      onDragOver: PropTypes.func.isRequired,
      onDragStart: PropTypes.func.isRequired,
      selectSource: PropTypes.func.isRequired,
      selectedLocation: PropTypes.object,
      showSource: PropTypes.func.isRequired,
      source: PropTypes.object.isRequired,
      sourceActor: PropTypes.object.isRequired,
      tabSources: PropTypes.array.isRequired,
      toggleBlackBox: PropTypes.func.isRequired,
      togglePrettyPrint: PropTypes.func.isRequired,
      isBlackBoxed: PropTypes.bool.isRequired,
    };
  }

  onTabContextMenu = (event, tab) => {
    event.preventDefault();
    this.showContextMenu(event, tab);
  };

  showContextMenu(e, tab) {
    const {
      cx,
      closeTab,
      closeTabs,
      copyToClipboard,
      tabSources,
      showSource,
      toggleBlackBox,
      togglePrettyPrint,
      selectedLocation,
      source,
      isBlackBoxed,
    } = this.props;

    const tabCount = tabSources.length;
    const otherTabs = tabSources.filter(t => t.id !== tab);
    const sourceTab = tabSources.find(t => t.id == tab);
    const tabURLs = tabSources.map(t => t.url);
    const otherTabURLs = otherTabs.map(t => t.url);

    if (!sourceTab || !selectedLocation || !selectedLocation.sourceId) {
      return;
    }

    const tabMenuItems = getTabMenuItems();
    const items = [
      {
        item: {
          ...tabMenuItems.closeTab,
          click: () => closeTab(cx, sourceTab),
        },
      },
      {
        item: {
          ...tabMenuItems.closeOtherTabs,
          click: () => closeTabs(cx, otherTabURLs),
          disabled: otherTabURLs.length === 0,
        },
      },
      {
        item: {
          ...tabMenuItems.closeTabsToEnd,
          click: () => {
            const tabIndex = tabSources.findIndex(t => t.id == tab);
            closeTabs(
              cx,
              tabURLs.filter((t, i) => i > tabIndex)
            );
          },
          disabled:
            tabCount === 1 ||
            tabSources.some((t, i) => t === tab && tabCount - 1 === i),
        },
      },
      {
        item: {
          ...tabMenuItems.closeAllTabs,
          click: () => closeTabs(cx, tabURLs),
        },
      },
      { item: { type: "separator" } },
      {
        item: {
          ...tabMenuItems.copySource,
          disabled: selectedLocation.sourceId !== tab,
          click: () => copyToClipboard(sourceTab),
        },
      },
      {
        item: {
          ...tabMenuItems.copySourceUri2,
          disabled: !selectedLocation.sourceUrl,
          click: () => copyToTheClipboard(getRawSourceURL(sourceTab.url)),
        },
      },
      {
        item: {
          ...tabMenuItems.showSource,
          disabled: !selectedLocation.sourceUrl,
          click: () => showSource(cx, tab),
        },
      },
      {
        item: {
          ...tabMenuItems.toggleBlackBox,
          label: isBlackBoxed
            ? L10N.getStr("ignoreContextItem.unignore")
            : L10N.getStr("ignoreContextItem.ignore"),
          disabled: !shouldBlackbox(source),
          click: () => toggleBlackBox(cx, source),
        },
      },
      {
        item: {
          ...tabMenuItems.prettyPrint,
          click: () => togglePrettyPrint(cx, tab),
          disabled: isPretty(sourceTab),
        },
      },
    ];

    showMenu(e, buildMenu(items));
  }

  isSourceSearchEnabled() {
    return this.props.activeSearch === "source";
  }

  render() {
    const {
      cx,
      selectedLocation,
      selectSource,
      closeTab,
      source,
      sourceActor,
      tabSources,
      onDragOver,
      onDragStart,
      onDragEnd,
    } = this.props;
    const sourceId = source.id;
    const active =
      selectedLocation &&
      sourceId == selectedLocation.sourceId &&
      !this.isSourceSearchEnabled();
    const isPrettyCode = isPretty(source);

    function onClickClose(e) {
      e.stopPropagation();
      closeTab(cx, source);
    }

    function handleTabClick(e) {
      e.preventDefault();
      e.stopPropagation();
      return selectSource(cx, source, sourceActor);
    }

    const className = classnames("source-tab", {
      active,
      pretty: isPrettyCode,
    });

    const path = getDisplayPath(source, tabSources);
    const query = getSourceQueryString(source);

    return (
      <div
        draggable
        onDragOver={onDragOver}
        onDragStart={onDragStart}
        onDragEnd={onDragEnd}
        className={className}
        key={sourceId}
        onClick={handleTabClick}
        // Accommodate middle click to close tab
        onMouseUp={e => e.button === 1 && closeTab(cx, source)}
        onContextMenu={e => this.onTabContextMenu(e, sourceId)}
        title={getFileURL(source, false)}
      >
        <SourceIcon
          source={source}
          forTab={true}
          modifier={icon =>
            ["file", "javascript"].includes(icon) ? null : icon
          }
        />
        <div className="filename">
          {getTruncatedFileName(source, query)}
          {path && <span>{`../${path}/..`}</span>}
        </div>
        <CloseButton
          handleClick={onClickClose}
          tooltip={L10N.getStr("sourceTabs.closeTabButtonTooltip")}
        />
      </div>
    );
  }
}

const mapStateToProps = (state, { source }) => {
  return {
    cx: getContext(state),
    tabSources: getSourcesForTabs(state),
    selectedLocation: getSelectedLocation(state),
    isBlackBoxed: isSourceBlackBoxed(state, source),
    activeSearch: getActiveSearch(state),
  };
};

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    copyToClipboard: actions.copyToClipboard,
    closeTab: actions.closeTab,
    closeTabs: actions.closeTabs,
    togglePrettyPrint: actions.togglePrettyPrint,
    showSource: actions.showSource,
    toggleBlackBox: actions.toggleBlackBox,
  },
  null,
  {
    withRef: true,
  }
)(Tab);
