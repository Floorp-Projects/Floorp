/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import { connect } from "../../utils/connect";

import { showMenu, buildMenu } from "devtools-contextmenu";

import SourceIcon from "../shared/SourceIcon";
import { CloseButton } from "../shared/Button";

import type { List } from "immutable";
import type { Source } from "../../types";

import actions from "../../actions";

import {
  getDisplayPath,
  getFileURL,
  getRawSourceURL,
  getSourceQueryString,
  getTruncatedFileName,
  isJavaScript,
  isPretty,
  shouldBlackbox
} from "../../utils/source";
import { copyToTheClipboard } from "../../utils/clipboard";
import { getTabMenuItems } from "../../utils/tabs";

import {
  getSelectedSource,
  getActiveSearch,
  getSourcesForTabs,
  getHasSiblingOfSameName
} from "../../selectors";
import type { ActiveSearchType } from "../../selectors";

import classnames from "classnames";

type SourcesList = List<Source>;

type Props = {
  tabSources: SourcesList,
  selectedSource: Source,
  source: Source,
  activeSearch: ActiveSearchType,
  hasSiblingOfSameName: boolean,
  selectSource: typeof actions.selectSource,
  closeTab: typeof actions.closeTab,
  closeTabs: typeof actions.closeTabs,
  togglePrettyPrint: typeof actions.togglePrettyPrint,
  showSource: typeof actions.showSource,
  toggleBlackBox: typeof actions.toggleBlackBox
};

class Tab extends PureComponent<Props> {
  onTabContextMenu = (event, tab: string) => {
    event.preventDefault();
    this.showContextMenu(event, tab);
  };

  showContextMenu(e, tab: string) {
    const {
      closeTab,
      closeTabs,
      tabSources,
      showSource,
      toggleBlackBox,
      togglePrettyPrint,
      selectedSource,
      source
    } = this.props;

    const tabCount = tabSources.length;
    const otherTabs = tabSources.filter(t => t.id !== tab);
    const sourceTab = tabSources.find(t => t.id == tab);
    const tabURLs = tabSources.map(t => t.url);
    const otherTabURLs = otherTabs.map(t => t.url);

    if (!sourceTab) {
      return;
    }

    const tabMenuItems = getTabMenuItems();
    const items = [
      {
        item: {
          ...tabMenuItems.closeTab,
          click: () => closeTab(sourceTab)
        }
      },
      {
        item: {
          ...tabMenuItems.closeOtherTabs,
          click: () => closeTabs(otherTabURLs),
          disabled: otherTabURLs.length === 0
        }
      },
      {
        item: {
          ...tabMenuItems.closeTabsToEnd,
          click: () => {
            const tabIndex = tabSources.findIndex(t => t.id == tab);
            closeTabs(tabURLs.filter((t, i) => i > tabIndex));
          },
          disabled:
            tabCount === 1 ||
            tabSources.some((t, i) => t === tab && tabCount - 1 === i)
        }
      },
      {
        item: { ...tabMenuItems.closeAllTabs, click: () => closeTabs(tabURLs) }
      },
      { item: { type: "separator" } },
      {
        item: {
          ...tabMenuItems.copyToClipboard,
          disabled: selectedSource.id !== tab,
          click: () => copyToTheClipboard(sourceTab.text)
        }
      },
      {
        item: {
          ...tabMenuItems.copySourceUri2,
          disabled: !selectedSource.url,
          click: () => copyToTheClipboard(getRawSourceURL(sourceTab.url))
        }
      },
      {
        item: {
          ...tabMenuItems.showSource,
          disabled: !selectedSource.url,
          click: () => showSource(tab)
        }
      },
      {
        item: {
          ...tabMenuItems.toggleBlackBox,
          label: source.isBlackBoxed
            ? L10N.getStr("sourceFooter.unblackbox")
            : L10N.getStr("sourceFooter.blackbox"),
          disabled: !shouldBlackbox(source),
          click: () => toggleBlackBox(source)
        }
      },
      {
        item: {
          ...tabMenuItems.prettyPrint,
          click: () => togglePrettyPrint(tab),
          disabled: isPretty(source) || !isJavaScript(source)
        }
      }
    ];

    showMenu(e, buildMenu(items));
  }

  isProjectSearchEnabled() {
    return this.props.activeSearch === "project";
  }

  isSourceSearchEnabled() {
    return this.props.activeSearch === "source";
  }

  render() {
    const {
      selectedSource,
      selectSource,
      closeTab,
      source,
      tabSources,
      hasSiblingOfSameName
    } = this.props;
    const sourceId = source.id;
    const active =
      selectedSource &&
      sourceId == selectedSource.id &&
      (!this.isProjectSearchEnabled() && !this.isSourceSearchEnabled());
    const isPrettyCode = isPretty(source);

    function onClickClose(e) {
      e.stopPropagation();
      closeTab(source);
    }

    function handleTabClick(e) {
      e.preventDefault();
      e.stopPropagation();
      return selectSource(sourceId);
    }

    const className = classnames("source-tab", {
      active,
      pretty: isPrettyCode
    });

    const path = getDisplayPath(source, tabSources);
    const query = hasSiblingOfSameName ? getSourceQueryString(source) : "";

    return (
      <div
        className={className}
        key={sourceId}
        onClick={handleTabClick}
        // Accommodate middle click to close tab
        onMouseUp={e => e.button === 1 && closeTab(source)}
        onContextMenu={e => this.onTabContextMenu(e, sourceId)}
        title={getFileURL(source, false)}
      >
        <SourceIcon
          source={source}
          shouldHide={icon => ["file", "javascript"].includes(icon)}
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
  const selectedSource = getSelectedSource(state);

  return {
    tabSources: getSourcesForTabs(state),
    selectedSource: selectedSource,
    activeSearch: getActiveSearch(state),
    hasSiblingOfSameName: getHasSiblingOfSameName(state, source)
  };
};

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    closeTab: actions.closeTab,
    closeTabs: actions.closeTabs,
    togglePrettyPrint: actions.togglePrettyPrint,
    showSource: actions.showSource,
    toggleBlackBox: actions.toggleBlackBox
  }
)(Tab);
