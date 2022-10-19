/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import classnames from "classnames";
import { showMenu } from "../../context-menu/menu";

import SourceIcon from "../shared/SourceIcon";
import AccessibleImage from "../shared/AccessibleImage";

import {
  getGeneratedSourceByURL,
  getContext,
  getSourceContent,
} from "../../selectors";
import actions from "../../actions";

import { shouldBlackbox, sourceTypes } from "../../utils/source";
import { copyToTheClipboard } from "../../utils/clipboard";
import { features } from "../../utils/prefs";
import { downloadFile } from "../../utils/utils";
import { isFulfilled } from "../../utils/async-value";

class SourceTreeItem extends Component {
  static get propTypes() {
    return {
      autoExpand: PropTypes.bool.isRequired,
      blackBoxSources: PropTypes.func.isRequired,
      clearProjectDirectoryRoot: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      depth: PropTypes.number.isRequired,
      expanded: PropTypes.bool.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.bool.isRequired,
      getBlackBoxSourcesGroups: PropTypes.func.isRequired,
      hasMatchingGeneratedSource: PropTypes.bool.isRequired,
      item: PropTypes.object.isRequired,
      loadSourceText: PropTypes.func.isRequired,
      projectRoot: PropTypes.string.isRequired,
      selectSourceItem: PropTypes.func.isRequired,
      setExpanded: PropTypes.func.isRequired,
      setProjectDirectoryRoot: PropTypes.func.isRequired,
      sourceContent: PropTypes.object,
      toggleBlackBox: PropTypes.func.isRequired,
      isSourceBlackBoxed: PropTypes.bool.isRequired,
      getParent: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    const { autoExpand, item } = this.props;
    if (autoExpand) {
      this.props.setExpanded(item, true, false);
    }
  }

  onClick = e => {
    const { item, focusItem, selectSourceItem } = this.props;

    focusItem(item);
    if (item.type == "source") {
      selectSourceItem(item);
    }
  };

  onContextMenu = event => {
    const copySourceUri2Label = L10N.getStr("copySourceUri2");
    const copySourceUri2Key = L10N.getStr("copySourceUri2.accesskey");
    const setDirectoryRootLabel = L10N.getStr("setDirectoryRoot.label");
    const setDirectoryRootKey = L10N.getStr("setDirectoryRoot.accesskey");
    const removeDirectoryRootLabel = L10N.getStr("removeDirectoryRoot.label");

    event.stopPropagation();
    event.preventDefault();

    const menuOptions = [];

    const { item } = this.props;
    if (item.type == "source") {
      const { source } = item;

      const copySourceUri2 = {
        id: "node-menu-copy-source",
        label: copySourceUri2Label,
        accesskey: copySourceUri2Key,
        disabled: false,
        click: () => copyToTheClipboard(source.url),
      };

      const { cx } = this.props;
      const blackBoxMenuItem = {
        id: "node-menu-blackbox",
        label: this.props.isSourceBlackBoxed
          ? L10N.getStr("ignoreContextItem.unignore")
          : L10N.getStr("ignoreContextItem.ignore"),
        accesskey: this.props.isSourceBlackBoxed
          ? L10N.getStr("ignoreContextItem.unignore.accesskey")
          : L10N.getStr("ignoreContextItem.ignore.accesskey"),
        disabled: !shouldBlackbox(source),
        click: () => this.props.toggleBlackBox(cx, source),
      };
      const downloadFileItem = {
        id: "node-menu-download-file",
        label: L10N.getStr("downloadFile.label"),
        accesskey: L10N.getStr("downloadFile.accesskey"),
        disabled: false,
        click: () => this.handleDownloadFile(cx, source),
      };
      menuOptions.push(copySourceUri2, blackBoxMenuItem, downloadFileItem);
    }

    // All other types other than source are folder-like
    if (item.type != "source") {
      this.addCollapseExpandAllOptions(menuOptions, item);

      if (features.root) {
        const { cx, depth, projectRoot } = this.props;

        if (projectRoot == item.uniquePath) {
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
            click: () =>
              this.props.setProjectDirectoryRoot(
                cx,
                item.uniquePath,
                this.renderItemName(depth)
              ),
          });
        }
      }

      this.addBlackboxAllOption(menuOptions, item);
    }

    showMenu(event, menuOptions);
  };

  handleDownloadFile = async (cx, source) => {
    if (!source) {
      return;
    }

    if (!this.props.sourceContent) {
      await this.props.loadSourceText({ cx, source });
    }
    const data = this.props.sourceContent;
    if (!data) {
      return;
    }
    downloadFile(data, source.displayURL.filename);
  };

  addBlackboxAllOption = (menuOptions, item) => {
    const { cx, depth, projectRoot } = this.props;
    const {
      sourcesInside,
      sourcesOutside,
      allInsideBlackBoxed,
      allOutsideBlackBoxed,
    } = this.props.getBlackBoxSourcesGroups(item);

    let blackBoxInsideMenuItemLabel;
    let blackBoxOutsideMenuItemLabel;
    if (depth === 0 || (depth === 1 && projectRoot === "")) {
      blackBoxInsideMenuItemLabel = allInsideBlackBoxed
        ? L10N.getStr("unignoreAllInGroup.label")
        : L10N.getStr("ignoreAllInGroup.label");
      if (sourcesOutside.length) {
        blackBoxOutsideMenuItemLabel = allOutsideBlackBoxed
          ? L10N.getStr("unignoreAllOutsideGroup.label")
          : L10N.getStr("ignoreAllOutsideGroup.label");
      }
    } else {
      blackBoxInsideMenuItemLabel = allInsideBlackBoxed
        ? L10N.getStr("unignoreAllInDir.label")
        : L10N.getStr("ignoreAllInDir.label");
      if (sourcesOutside.length) {
        blackBoxOutsideMenuItemLabel = allOutsideBlackBoxed
          ? L10N.getStr("unignoreAllOutsideDir.label")
          : L10N.getStr("ignoreAllOutsideDir.label");
      }
    }

    const blackBoxInsideMenuItem = {
      id: allInsideBlackBoxed
        ? "node-unblackbox-all-inside"
        : "node-blackbox-all-inside",
      label: blackBoxInsideMenuItemLabel,
      disabled: false,
      click: () =>
        this.props.blackBoxSources(cx, sourcesInside, !allInsideBlackBoxed),
    };

    if (sourcesOutside.length) {
      menuOptions.push({
        id: "node-blackbox-all",
        label: L10N.getStr("ignoreAll.label"),
        submenu: [
          blackBoxInsideMenuItem,
          {
            id: allOutsideBlackBoxed
              ? "node-unblackbox-all-outside"
              : "node-blackbox-all-outside",
            label: blackBoxOutsideMenuItemLabel,
            disabled: false,
            click: () =>
              this.props.blackBoxSources(
                cx,
                sourcesOutside,
                !allOutsideBlackBoxed
              ),
          },
        ],
      });
    } else {
      menuOptions.push(blackBoxInsideMenuItem);
    }
  };

  addCollapseExpandAllOptions = (menuOptions, item) => {
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
    return item.type != "source" ? (
      <AccessibleImage className={classnames("arrow", { expanded })} />
    ) : (
      <span className="img no-arrow" />
    );
  }

  renderIcon(item, depth) {
    if (item.type == "thread") {
      const icon = item.thread.targetType.includes("worker")
        ? "worker"
        : "window";
      return <AccessibleImage className={classnames(icon)} />;
    }
    if (item.type == "group") {
      if (item.groupName === "Webpack") {
        return <AccessibleImage className="webpack" />;
      } else if (item.groupName === "Angular") {
        return <AccessibleImage className="angular" />;
      }
      // Check if the group relates to an extension.
      // This happens when a webextension injects a content script.
      if (item.isForExtensionSource) {
        return <AccessibleImage className="extension" />;
      }

      return <AccessibleImage className="globe-small" />;
    }
    if (item.type == "directory") {
      return <AccessibleImage className="folder" />;
    }
    if (item.type == "source") {
      const { source } = item;
      return (
        <SourceIcon
          source={source}
          modifier={icon =>
            // In the SourceTree, extension files should use the file-extension based icon,
            // whereas we use the extension icon in other Components (eg. source tabs and breakpoints pane).
            icon === "extension"
              ? sourceTypes[source.displayURL.fileExtension] || "javascript"
              : icon
          }
        />
      );
    }

    return null;
  }

  renderItemName(depth) {
    const { item } = this.props;

    if (item.type == "thread") {
      const { thread } = item;
      return (
        thread.name +
        (thread.serviceWorkerStatus ? ` (${thread.serviceWorkerStatus})` : "")
      );
    }
    if (item.type == "group") {
      return unescape(item.groupName);
    }
    if (item.type == "directory") {
      const parentItem = this.props.getParent(item);
      return item.path.replace(parentItem.path, "").replace(/^\//, "");
    }
    if (item.type == "source") {
      const { displayURL } = item.source;
      const name =
        displayURL.filename + (displayURL.search ? displayURL.search : "");
      return unescape(name);
    }

    return null;
  }

  renderItemTooltip() {
    const { item } = this.props;

    if (item.type == "thread") {
      return item.thread.name;
    }
    if (item.type == "group") {
      return item.groupName;
    }
    if (item.type == "directory") {
      return item.path;
    }
    if (item.type == "source") {
      return item.source.url;
    }

    return null;
  }

  render() {
    const { item, depth, focused, hasMatchingGeneratedSource } = this.props;

    const suffix = hasMatchingGeneratedSource ? (
      <span className="suffix">{L10N.getStr("sourceFooter.mappedSuffix")}</span>
    ) : null;

    return (
      <div
        className={classnames("node", { focused })}
        key={item.path}
        onClick={this.onClick}
        onContextMenu={this.onContextMenu}
        title={this.renderItemTooltip()}
      >
        {this.renderItemArrow()}
        {this.renderIcon(item, depth)}
        <span className="label">
          {this.renderItemName(depth)}
          {suffix}
        </span>
      </div>
    );
  }
}

function getHasMatchingGeneratedSource(state, source) {
  if (!source || !source.isOriginal) {
    return false;
  }

  return !!getGeneratedSourceByURL(state, source.url);
}

function getSourceContentValue(state, source) {
  const content = getSourceContent(state, source.id);
  return content && isFulfilled(content) ? content.value : null;
}

const mapStateToProps = (state, props) => {
  const { item } = props;
  if (item.type == "source") {
    const { source } = item;
    return {
      cx: getContext(state),
      hasMatchingGeneratedSource: getHasMatchingGeneratedSource(state, source),
      sourceContent: getSourceContentValue(state, source),
    };
  }
  return {
    cx: getContext(state),
  };
};

export default connect(mapStateToProps, {
  setProjectDirectoryRoot: actions.setProjectDirectoryRoot,
  clearProjectDirectoryRoot: actions.clearProjectDirectoryRoot,
  toggleBlackBox: actions.toggleBlackBox,
  loadSourceText: actions.loadSourceText,
  blackBoxSources: actions.blackBoxSources,
  setBlackBoxAllOutside: actions.setBlackBoxAllOutside,
})(SourceTreeItem);
