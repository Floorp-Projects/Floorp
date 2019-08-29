/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../utils/connect";
import classnames from "classnames";
import { showMenu } from "devtools-contextmenu";

import SourceIcon from "../shared/SourceIcon";
import AccessibleImage from "../shared/AccessibleImage";
import { isWorker } from "../../utils/threads";

import {
  getGeneratedSourceByURL,
  getHasSiblingOfSameName,
  hasPrettySource as checkHasPrettySource,
  getContext,
  getMainThread,
  getExtensionNameBySourceUrl,
  getSourceContent,
} from "../../selectors";
import actions from "../../actions";

import {
  isOriginal as isOriginalSource,
  getSourceQueryString,
  isUrlExtension,
  shouldBlackbox,
} from "../../utils/source";
import { isDirectory, getPathWithoutThread } from "../../utils/sources-tree";
import { copyToTheClipboard } from "../../utils/clipboard";
import { features } from "../../utils/prefs";
import { downloadFile } from "../../utils/utils";

import type { TreeNode } from "../../utils/sources-tree/types";
import type { Source, Context, Thread, SourceContent } from "../../types";

type Props = {
  autoExpand: ?boolean,
  cx: Context,
  debuggeeUrl: string,
  projectRoot: string,
  source: ?Source,
  extensionName: string | null,
  item: TreeNode,
  sourceContent: SourceContent,
  depth: number,
  focused: boolean,
  expanded: boolean,
  threads: Thread[],
  mainThread: Thread,
  hasMatchingGeneratedSource: boolean,
  hasSiblingOfSameName: boolean,
  hasPrettySource: boolean,
  focusItem: TreeNode => void,
  selectItem: TreeNode => void,
  setExpanded: (TreeNode, boolean, boolean) => void,
  clearProjectDirectoryRoot: typeof actions.clearProjectDirectoryRoot,
  setProjectDirectoryRoot: typeof actions.setProjectDirectoryRoot,
  toggleBlackBox: typeof actions.toggleBlackBox,
  loadSourceText: typeof actions.loadSourceText,
};

type State = {};

type MenuOption = {
  id: string,
  label: string,
  disabled: boolean,
  click: () => any,
};

type ContextMenu = Array<MenuOption>;

class SourceTreeItem extends Component<Props, State> {
  componentDidMount() {
    const { autoExpand, item } = this.props;
    if (autoExpand) {
      this.props.setExpanded(item, true, false);
    }
  }

  onClick = (e: MouseEvent) => {
    const { item, focusItem, selectItem } = this.props;

    focusItem(item);
    if (!isDirectory(item)) {
      selectItem(item);
    }
  };

  onContextMenu = (event: Event, item: TreeNode) => {
    const copySourceUri2Label = L10N.getStr("copySourceUri2");
    const copySourceUri2Key = L10N.getStr("copySourceUri2.accesskey");
    const setDirectoryRootLabel = L10N.getStr("setDirectoryRoot.label");
    const setDirectoryRootKey = L10N.getStr("setDirectoryRoot.accesskey");
    const removeDirectoryRootLabel = L10N.getStr("removeDirectoryRoot.label");

    event.stopPropagation();
    event.preventDefault();

    const menuOptions = [];

    if (!isDirectory(item)) {
      // Flow requires some extra handling to ensure the value of contents.
      const { contents } = item;

      if (!Array.isArray(contents)) {
        const copySourceUri2 = {
          id: "node-menu-copy-source",
          label: copySourceUri2Label,
          accesskey: copySourceUri2Key,
          disabled: false,
          click: () => copyToTheClipboard(contents.url),
        };

        const { cx, source } = this.props;
        if (source) {
          const blackBoxMenuItem = {
            id: "node-menu-blackbox",
            label: source.isBlackBoxed
              ? L10N.getStr("blackboxContextItem.unblackbox")
              : L10N.getStr("blackboxContextItem.blackbox"),
            accesskey: source.isBlackBoxed
              ? L10N.getStr("blackboxContextItem.unblackbox.accesskey")
              : L10N.getStr("blackboxContextItem.blackbox.accesskey"),
            disabled: !shouldBlackbox(source),
            click: () => this.props.toggleBlackBox(cx, source),
          };
          const downloadFileItem = {
            id: "node-menu-download-file",
            label: L10N.getStr("downloadFile.label"),
            accesskey: L10N.getStr("downloadFile.accesskey"),
            disabled: false,
            click: () => this.handleDownloadFile(cx, source, item),
          };
          menuOptions.push(copySourceUri2, blackBoxMenuItem, downloadFileItem);
        }
      }
    }

    if (isDirectory(item)) {
      this.addCollapseExpandAllOptions(menuOptions, item);

      if (features.root) {
        const { path } = item;
        const { cx, projectRoot } = this.props;

        if (projectRoot.endsWith(path)) {
          menuOptions.push({
            id: "node-remove-directory-root",
            label: removeDirectoryRootLabel,
            disabled: false,
            click: () => this.props.clearProjectDirectoryRoot(cx),
          });
        } else {
          menuOptions.push({
            id: "node-set-directory-root",
            label: setDirectoryRootLabel,
            accesskey: setDirectoryRootKey,
            disabled: false,
            click: () => this.props.setProjectDirectoryRoot(cx, path),
          });
        }
      }
    }

    showMenu(event, menuOptions);
  };

  handleDownloadFile = async (cx: Context, source: ?Source, item: TreeNode) => {
    const name = item.name;
    if (!this.props.sourceContent) {
      await this.props.loadSourceText({ cx, source });
    }
    const data = this.props.sourceContent;
    downloadFile(data, name);
  };

  addCollapseExpandAllOptions = (menuOptions: ContextMenu, item: TreeNode) => {
    const { setExpanded } = this.props;

    menuOptions.push({
      id: "node-menu-collapse-all",
      label: L10N.getStr("collapseAll.label"),
      disabled: false,
      click: () => setExpanded(item, false, true),
    });

    menuOptions.push({
      id: "node-menu-expand-all",
      label: L10N.getStr("expandAll.label"),
      disabled: false,
      click: () => setExpanded(item, true, true),
    });
  };

  renderItemArrow() {
    const { item, expanded } = this.props;
    return isDirectory(item) ? (
      <AccessibleImage className={classnames("arrow", { expanded })} />
    ) : (
      <span className="img no-arrow" />
    );
  }

  renderIcon(item: TreeNode, depth: number) {
    const {
      debuggeeUrl,
      projectRoot,
      source,
      hasPrettySource,
      threads,
    } = this.props;

    if (item.name === "webpack://") {
      return <AccessibleImage className="webpack" />;
    } else if (item.name === "ng://") {
      return <AccessibleImage className="angular" />;
    } else if (isUrlExtension(item.path) && depth === 1) {
      return <AccessibleImage className="extension" />;
    }

    // Threads level
    if (depth === 0 && projectRoot === "") {
      const thread = threads.find(thrd => thrd.actor == item.name);

      if (thread) {
        const icon = isWorker(thread) ? "worker" : "window";
        return (
          <AccessibleImage
            className={classnames(icon, {
              debuggee: debuggeeUrl && debuggeeUrl.includes(item.name),
            })}
          />
        );
      }
    }

    if (isDirectory(item)) {
      // Domain level
      if (depth === 1 && projectRoot === "") {
        return <AccessibleImage className="globe-small" />;
      }
      return <AccessibleImage className="folder" />;
    }

    if (source && source.isBlackBoxed) {
      return <AccessibleImage className="blackBox" />;
    }

    if (hasPrettySource) {
      return <AccessibleImage className="prettyPrint" />;
    }

    if (source) {
      return (
        <SourceIcon source={source} shouldHide={icon => icon === "extension"} />
      );
    }

    return null;
  }

  renderItemName(depth) {
    const { item, threads, extensionName } = this.props;

    if (depth === 0) {
      const thread = threads.find(({ actor }) => actor == item.name);
      if (thread) {
        return thread.name;
      }
    }

    if (isExtensionDirectory(depth, extensionName)) {
      return extensionName;
    }

    switch (item.name) {
      case "ng://":
        return "Angular";
      case "webpack://":
        return "Webpack";
      default:
        return `${unescape(item.name)}`;
    }
  }

  renderItemTooltip() {
    const { item, depth, extensionName } = this.props;

    if (isExtensionDirectory(depth, extensionName)) {
      return item.name;
    }

    return item.type === "source"
      ? unescape(item.contents.url)
      : getPathWithoutThread(item.path);
  }

  render() {
    const {
      item,
      depth,
      source,
      focused,
      hasMatchingGeneratedSource,
      hasSiblingOfSameName,
    } = this.props;

    const suffix = hasMatchingGeneratedSource ? (
      <span className="suffix">{L10N.getStr("sourceFooter.mappedSuffix")}</span>
    ) : null;

    let querystring;
    if (hasSiblingOfSameName) {
      querystring = getSourceQueryString(source);
    }

    const query =
      hasSiblingOfSameName && querystring ? (
        <span className="query">{querystring}</span>
      ) : null;

    return (
      <div
        className={classnames("node", { focused })}
        key={item.path}
        onClick={this.onClick}
        onContextMenu={e => this.onContextMenu(e, item)}
        title={this.renderItemTooltip()}
      >
        {this.renderItemArrow()}
        {this.renderIcon(item, depth)}
        <span className="label">
          {this.renderItemName(depth)}
          {query} {suffix}
        </span>
      </div>
    );
  }
}

function getHasMatchingGeneratedSource(state, source: ?Source) {
  if (!source || !isOriginalSource(source)) {
    return false;
  }

  return !!getGeneratedSourceByURL(state, source.url);
}

function getSourceContentValue(state, source: Source) {
  const content = getSourceContent(state, source.id);
  return content !== null ? content.value : false;
}

function isExtensionDirectory(depth, extensionName) {
  return extensionName && depth === 1;
}

const mapStateToProps = (state, props) => {
  const { source, item } = props;
  return {
    cx: getContext(state),
    mainThread: getMainThread(state),
    hasMatchingGeneratedSource: getHasMatchingGeneratedSource(state, source),
    hasSiblingOfSameName: getHasSiblingOfSameName(state, source),
    hasPrettySource: source ? checkHasPrettySource(state, source.id) : false,
    sourceContent: source ? getSourceContentValue(state, source) : false,
    extensionName:
      (isUrlExtension(item.name) &&
        getExtensionNameBySourceUrl(state, item.name)) ||
      null,
  };
};

export default connect(
  mapStateToProps,
  {
    setProjectDirectoryRoot: actions.setProjectDirectoryRoot,
    clearProjectDirectoryRoot: actions.clearProjectDirectoryRoot,
    toggleBlackBox: actions.toggleBlackBox,
    loadSourceText: actions.loadSourceText,
  }
)(SourceTreeItem);
