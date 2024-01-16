/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import SourceIcon from "../shared/SourceIcon";
import AccessibleImage from "../shared/AccessibleImage";

import {
  getGeneratedSourceByURL,
  isSourceOverridden,
  getHideIgnoredSources,
} from "../../selectors";
import actions from "../../actions";

import { sourceTypes } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { safeDecodeItemName } from "../../utils/sources-tree/utils";

const classnames = require("devtools/client/shared/classnames.js");

class SourceTreeItem extends Component {
  static get propTypes() {
    return {
      autoExpand: PropTypes.bool.isRequired,
      depth: PropTypes.bool.isRequired,
      expanded: PropTypes.bool.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.bool.isRequired,
      hasMatchingGeneratedSource: PropTypes.bool.isRequired,
      item: PropTypes.object.isRequired,
      selectSourceItem: PropTypes.func.isRequired,
      setExpanded: PropTypes.func.isRequired,
      getParent: PropTypes.func.isRequired,
      isOverridden: PropTypes.bool,
      hideIgnoredSources: PropTypes.bool,
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
    event.stopPropagation();
    event.preventDefault();
    this.props.showSourceTreeItemContextMenu(
      event,
      this.props.item,
      this.props.depth,
      this.props.setExpanded,
      this.renderItemName()
    );
  };

  renderItemArrow() {
    const { item, expanded } = this.props;
    return item.type != "source"
      ? React.createElement(AccessibleImage, {
          className: classnames("arrow", {
            expanded,
          }),
        })
      : span({
          className: "img no-arrow",
        });
  }

  renderIcon(item) {
    if (item.type == "thread") {
      const icon = item.thread.targetType.includes("worker")
        ? "worker"
        : "window";
      return React.createElement(AccessibleImage, {
        className: classnames(icon),
      });
    }
    if (item.type == "group") {
      if (item.groupName === "Webpack") {
        return React.createElement(AccessibleImage, {
          className: "webpack",
        });
      } else if (item.groupName === "Angular") {
        return React.createElement(AccessibleImage, {
          className: "angular",
        });
      }
      // Check if the group relates to an extension.
      // This happens when a webextension injects a content script.
      if (item.isForExtensionSource) {
        return React.createElement(AccessibleImage, {
          className: "extension",
        });
      }
      return React.createElement(AccessibleImage, {
        className: "globe-small",
      });
    }
    if (item.type == "directory") {
      return React.createElement(AccessibleImage, {
        className: "folder",
      });
    }
    if (item.type == "source") {
      const { source, sourceActor } = item;
      return React.createElement(SourceIcon, {
        location: createLocation({
          source,
          sourceActor,
        }),
        modifier: icon => {
          // In the SourceTree, extension files should use the file-extension based icon,
          // whereas we use the extension icon in other Components (eg. source tabs and breakpoints pane).
          if (icon === "extension") {
            return sourceTypes[source.displayURL.fileExtension] || "javascript";
          }
          return icon + (this.props.isOverridden ? " override" : "");
        },
      });
    }
    return null;
  }
  renderItemName() {
    const { item } = this.props;

    if (item.type == "thread") {
      const { thread } = item;
      return (
        thread.name +
        (thread.serviceWorkerStatus ? ` (${thread.serviceWorkerStatus})` : "")
      );
    }
    if (item.type == "group") {
      return safeDecodeItemName(item.groupName);
    }
    if (item.type == "directory") {
      const parentItem = this.props.getParent(item);
      return safeDecodeItemName(
        item.path.replace(parentItem.path, "").replace(/^\//, "")
      );
    }
    if (item.type == "source") {
      const { displayURL } = item.source;
      const name =
        displayURL.filename + (displayURL.search ? displayURL.search : "");
      return safeDecodeItemName(name);
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
    const { item, focused, hasMatchingGeneratedSource, hideIgnoredSources } =
      this.props;

    if (hideIgnoredSources && item.isBlackBoxed) {
      return null;
    }
    const suffix = hasMatchingGeneratedSource
      ? span(
          {
            className: "suffix",
          },
          L10N.getStr("sourceFooter.mappedSuffix")
        )
      : null;
    return div(
      {
        className: classnames("node", {
          focused,
          blackboxed: item.type == "source" && item.isBlackBoxed,
        }),
        key: item.path,
        onClick: this.onClick,
        onContextMenu: this.onContextMenu,
        title: this.renderItemTooltip(),
      },
      this.renderItemArrow(),
      this.renderIcon(item),
      span(
        {
          className: "label",
        },
        this.renderItemName(),
        suffix
      )
    );
  }
}

function getHasMatchingGeneratedSource(state, source) {
  if (!source || !source.isOriginal) {
    return false;
  }

  return !!getGeneratedSourceByURL(state, source.url);
}

const mapStateToProps = (state, props) => {
  const { item } = props;
  if (item.type == "source") {
    const { source } = item;
    return {
      hasMatchingGeneratedSource: getHasMatchingGeneratedSource(state, source),
      isOverridden: isSourceOverridden(state, source),
      hideIgnoredSources: getHideIgnoredSources(state),
    };
  }
  return {};
};

export default connect(mapStateToProps, {
  showSourceTreeItemContextMenu: actions.showSourceTreeItemContextMenu,
})(SourceTreeItem);
