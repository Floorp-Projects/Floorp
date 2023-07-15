/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

// Selectors
import {
  getSelectedLocation,
  getMainThreadHost,
  getExpandedState,
  getProjectDirectoryRoot,
  getProjectDirectoryRootName,
  getSourcesTreeSources,
  getFocusedSourceItem,
  getGeneratedSourceByURL,
  getHideIgnoredSources,
} from "../../selectors";

// Actions
import actions from "../../actions";

// Components
import SourcesTreeItem from "./SourcesTreeItem";
import AccessibleImage from "../shared/AccessibleImage";

// Utils
import { getRawSourceURL } from "../../utils/source";
import { createLocation } from "../../utils/location";

const classnames = require("devtools/client/shared/classnames.js");
const Tree = require("devtools/client/shared/components/Tree");

function shouldAutoExpand(item, mainThreadHost) {
  // There is only one case where we want to force auto expand,
  // when we are on the group of the page's domain.
  return item.type == "group" && item.groupName === mainThreadHost;
}

/**
 * Get the SourceItem displayed in the SourceTree for a given "tree location".
 *
 * @param {Object} treeLocation
 *        An object containing  the Source coming from the sources.js reducer and the source actor
 *        See getTreeLocation().
 * @param {object} rootItems
 *        Result of getSourcesTreeSources selector, containing all sources sorted in a tree structure.
 *        items to be displayed in the source tree.
 * @return {SourceItem}
 *        The directory source item where the given source is displayed.
 */
function getSourceItemForTreeLocation(treeLocation, rootItems) {
  // Sources without URLs are not visible in the SourceTree
  const { source, sourceActor } = treeLocation;

  if (!source.url) {
    return null;
  }
  const { displayURL } = source;
  function findSourceInItem(item, path) {
    if (item.type == "source") {
      if (item.source.url == source.url) {
        return item;
      }
      return null;
    }
    // Bail out if we the current item doesn't match the source
    if (item.type == "thread" && item.threadActorID != sourceActor?.thread) {
      return null;
    }
    if (item.type == "group" && displayURL.group != item.groupName) {
      return null;
    }
    if (item.type == "directory" && !path.startsWith(item.path)) {
      return null;
    }
    // Otherwise, walk down the tree if this ancestor item seems to match
    for (const child of item.children) {
      const match = findSourceInItem(child, path);
      if (match) {
        return match;
      }
    }

    return null;
  }
  for (const rootItem of rootItems) {
    // Note that when we are setting a project root, rootItem
    // may no longer be only Thread Item, but also be Group, Directory or Source Items.
    const item = findSourceInItem(rootItem, displayURL.path);
    if (item) {
      return item;
    }
  }
  return null;
}

class SourcesTree extends Component {
  constructor(props) {
    super(props);

    this.state = {};
  }

  static get propTypes() {
    return {
      mainThreadHost: PropTypes.string.isRequired,
      expanded: PropTypes.object.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.object,
      projectRoot: PropTypes.string.isRequired,
      selectSource: PropTypes.func.isRequired,
      selectedTreeLocation: PropTypes.object,
      setExpandedState: PropTypes.func.isRequired,
      rootItems: PropTypes.object.isRequired,
      clearProjectDirectoryRoot: PropTypes.func.isRequired,
      projectRootName: PropTypes.string.isRequired,
      setHideOrShowIgnoredSources: PropTypes.func.isRequired,
      hideIgnoredSources: PropTypes.bool.isRequired,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { selectedTreeLocation } = this.props;

    // We might fail to find the source if its thread is registered late,
    // so that we should re-search the selected source if state.focused is null.
    if (
      nextProps.selectedTreeLocation?.source &&
      (nextProps.selectedTreeLocation.source != selectedTreeLocation?.source ||
        (nextProps.selectedTreeLocation.source ===
          selectedTreeLocation?.source &&
          nextProps.selectedTreeLocation.sourceActor !=
            selectedTreeLocation?.sourceActor) ||
        !this.props.focused)
    ) {
      const sourceItem = getSourceItemForTreeLocation(
        nextProps.selectedTreeLocation,
        this.props.rootItems
      );
      if (sourceItem) {
        // Walk up the tree to expand all ancestor items up to the root of the tree.
        const expanded = new Set(this.props.expanded);
        let parentDirectory = sourceItem;
        while (parentDirectory) {
          expanded.add(this.getKey(parentDirectory));
          parentDirectory = this.getParent(parentDirectory);
        }
        this.props.setExpandedState(expanded);
        this.onFocus(sourceItem);
      }
    }
  }

  selectSourceItem = item => {
    this.props.selectSource(item.source, item.sourceActor);
  };

  onFocus = item => {
    this.props.focusItem(item);
  };

  onActivate = item => {
    if (item.type == "source") {
      this.selectSourceItem(item);
    }
  };

  onExpand = (item, shouldIncludeChildren) => {
    this.setExpanded(item, true, shouldIncludeChildren);
  };

  onCollapse = (item, shouldIncludeChildren) => {
    this.setExpanded(item, false, shouldIncludeChildren);
  };

  setExpanded = (item, isExpanded, shouldIncludeChildren) => {
    const { expanded } = this.props;
    let changed = false;
    const expandItem = i => {
      const key = this.getKey(i);
      if (isExpanded) {
        changed |= !expanded.has(key);
        expanded.add(key);
      } else {
        changed |= expanded.has(key);
        expanded.delete(key);
      }
    };
    expandItem(item);

    if (shouldIncludeChildren) {
      let parents = [item];
      while (parents.length) {
        const children = [];
        for (const parent of parents) {
          for (const child of this.getChildren(parent)) {
            expandItem(child);
            children.push(child);
          }
        }
        parents = children;
      }
    }
    if (changed) {
      this.props.setExpandedState(expanded);
    }
  };

  isEmpty() {
    return !this.getRoots().length;
  }

  renderEmptyElement(message) {
    return (
      <div key="empty" className="no-sources-message">
        {message}
      </div>
    );
  }

  getRoots = () => {
    return this.props.rootItems;
  };

  getKey = item => {
    // As this is used as React key in Tree component,
    // we need to update the key when switching to a new project root
    // otherwise these items won't be updated and will have a buggy padding start.
    const { projectRoot } = this.props;
    if (projectRoot) {
      return projectRoot + item.uniquePath;
    }
    return item.uniquePath;
  };

  getChildren = item => {
    // This is the precial magic that coalesce "empty" folders,
    // i.e folders which have only one sub-folder as children.
    function skipEmptyDirectories(directory) {
      if (directory.type != "directory") {
        return directory;
      }
      if (
        directory.children.length == 1 &&
        directory.children[0].type == "directory"
      ) {
        return skipEmptyDirectories(directory.children[0]);
      }
      return directory;
    }
    if (item.type == "thread") {
      return item.children;
    } else if (item.type == "group" || item.type == "directory") {
      return item.children.map(skipEmptyDirectories);
    }
    return [];
  };

  getParent = item => {
    if (item.type == "thread") {
      return null;
    }
    const { rootItems } = this.props;
    // This is the second magic which skip empty folders
    // (See getChildren comment)
    function skipEmptyDirectories(directory) {
      if (
        directory.type == "group" ||
        directory.type == "thread" ||
        rootItems.includes(directory)
      ) {
        return directory;
      }
      if (
        directory.children.length == 1 &&
        directory.children[0].type == "directory"
      ) {
        return skipEmptyDirectories(directory.parent);
      }
      return directory;
    }
    return skipEmptyDirectories(item.parent);
  };

  renderProjectRootHeader() {
    const { projectRootName } = this.props;

    if (!projectRootName) {
      return null;
    }

    return (
      <div key="root" className="sources-clear-root-container">
        <button
          className="sources-clear-root"
          onClick={() => this.props.clearProjectDirectoryRoot()}
          title={L10N.getStr("removeDirectoryRoot.label")}
        >
          <AccessibleImage className="home" />
          <AccessibleImage className="breadcrumb" />
          <span className="sources-clear-root-label">{projectRootName}</span>
        </button>
      </div>
    );
  }

  renderItem = (item, depth, focused, _, expanded) => {
    const { mainThreadHost } = this.props;
    return (
      <SourcesTreeItem
        item={item}
        depth={depth}
        focused={focused}
        autoExpand={shouldAutoExpand(item, mainThreadHost)}
        expanded={expanded}
        focusItem={this.onFocus}
        selectSourceItem={this.selectSourceItem}
        setExpanded={this.setExpanded}
        getParent={this.getParent}
      />
    );
  };

  renderTree() {
    const { expanded, focused } = this.props;

    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: 1,
      expanded,
      focused,
      getChildren: this.getChildren,
      getParent: this.getParent,
      getKey: this.getKey,
      getRoots: this.getRoots,
      itemHeight: 21,
      key: this.isEmpty() ? "empty" : "full",
      onCollapse: this.onCollapse,
      onExpand: this.onExpand,
      onFocus: this.onFocus,
      isExpanded: item => {
        return this.props.expanded.has(this.getKey(item));
      },
      onActivate: this.onActivate,
      renderItem: this.renderItem,
      preventBlur: true,
    };

    return <Tree {...treeProps} />;
  }

  renderPane(child) {
    const { projectRoot } = this.props;

    return (
      <div
        key="pane"
        className={classnames("sources-pane", {
          "sources-list-custom-root": !!projectRoot,
        })}
      >
        {child}
      </div>
    );
  }

  renderFooter() {
    if (this.props.hideIgnoredSources) {
      return (
        <footer className="source-list-footer">
          {L10N.getStr("ignoredSourcesHidden")}
          <button
            className="devtools-togglebutton"
            onClick={() => this.props.setHideOrShowIgnoredSources(false)}
            title={L10N.getStr("showIgnoredSources.tooltip.label")}
          >
            {L10N.getStr("showIgnoredSources")}
          </button>
        </footer>
      );
    }
    return null;
  }

  render() {
    const { projectRoot } = this.props;
    return (
      <div
        key="pane"
        className={classnames("sources-list", {
          "sources-list-custom-root": !!projectRoot,
        })}
      >
        {this.isEmpty() ? (
          this.renderEmptyElement(L10N.getStr("noSourcesText"))
        ) : (
          <>
            {this.renderProjectRootHeader()}
            {this.renderTree()}
            {this.renderFooter()}
          </>
        )}
      </div>
    );
  }
}

function getTreeLocation(state, location) {
  // In the SourceTree, we never show the pretty printed sources and only
  // the minified version, so if we are selecting a pretty file, fake selecting
  // the minified version.
  if (location?.source.isPrettyPrinted) {
    const source = getGeneratedSourceByURL(
      state,
      getRawSourceURL(location.source.url)
    );
    if (source) {
      return createLocation({
        source,
        // A source actor is required by getSourceItemForTreeLocation
        // in order to know in which thread this source relates to.
        sourceActor: location.sourceActor,
      });
    }
  }
  return location;
}

const mapStateToProps = state => {
  return {
    selectedTreeLocation: getTreeLocation(state, getSelectedLocation(state)),
    mainThreadHost: getMainThreadHost(state),
    expanded: getExpandedState(state),
    focused: getFocusedSourceItem(state),
    projectRoot: getProjectDirectoryRoot(state),
    rootItems: getSourcesTreeSources(state),
    projectRootName: getProjectDirectoryRootName(state),
    hideIgnoredSources: getHideIgnoredSources(state),
  };
};

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  setExpandedState: actions.setExpandedState,
  focusItem: actions.focusItem,
  clearProjectDirectoryRoot: actions.clearProjectDirectoryRoot,
  setHideOrShowIgnoredSources: actions.setHideOrShowIgnoredSources,
})(SourcesTree);
