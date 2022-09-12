/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
import React, { Component } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";
import { connect } from "../../utils/connect";

// Selectors
import {
  getSelectedSource,
  getMainThreadHost,
  getExpandedState,
  getProjectDirectoryRoot,
  getSourcesTreeSources,
  getFocusedSourceItem,
  getContext,
  getGeneratedSourceByURL,
  getBlackBoxRanges,
} from "../../selectors";

// Actions
import actions from "../../actions";

// Components
import SourcesTreeItem from "./SourcesTreeItem";
import ManagedTree from "../shared/ManagedTree";

// Utils
import { getRawSourceURL } from "../../utils/source";

function shouldAutoExpand(item, mainThreadHost) {
  // There is only one case where we want to force auto expand,
  // when we are on the group of the page's domain.
  return item.type == "group" && item.groupName === mainThreadHost;
}

/**
 * Get the one directory item where the given source is meant to be displayed in the SourceTree.
 *
 * @param {Source} source
 *        Source object coming from the sources.js reducer
 * @param {object} rootItems
 *        Result of getSourcesTreeSources selector, containing all sources sorted in a tree structure.
 *        items to be displayed in the source tree.
 * @return {SourceItem}
 *        The directory source item where the given source is displayed.
 */
function getDirectoryForSource(source, rootItems) {
  // Sources without URLs are not visible in the SourceTree
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
    if (item.type == "thread" && source.thread != item.thread.actor) {
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
      cx: PropTypes.object.isRequired,
      mainThreadHost: PropTypes.string.isRequired,
      expanded: PropTypes.object.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.object,
      projectRoot: PropTypes.string.isRequired,
      selectSource: PropTypes.func.isRequired,
      selectedSource: PropTypes.object,
      setExpandedState: PropTypes.func.isRequired,
      blackBoxRanges: PropTypes.object.isRequired,
      rootItems: PropTypes.object.isRequired,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { selectedSource } = this.props;

    // We might fail to find the source if its thread is registered late,
    // so that we should re-search the selected source if highlightItems is empty.
    if (
      nextProps.selectedSource &&
      (nextProps.selectedSource != selectedSource ||
        !this.state.highlightItems?.length)
    ) {
      let parentDirectory = getDirectoryForSource(
        nextProps.selectedSource,
        this.props.rootItems
      );
      // As highlightItems has to contains *all* the expanded items,
      // walk up the tree to put all ancestor items up to the root of the tree.
      const highlightItems = [];
      while (parentDirectory) {
        highlightItems.push(parentDirectory);
        parentDirectory = this.getParent(parentDirectory);
      }
      this.setState({ highlightItems });
    }
  }

  selectSourceItem = item => {
    this.props.selectSource(this.props.cx, item.source.id);
  };

  onFocus = item => {
    this.props.focusItem(item);
  };

  onActivate = item => {
    if (item.type == "source") {
      this.selectSourceItem(item);
    }
  };

  onExpand = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
  };

  onCollapse = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
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

  getPath = item => {
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

  /**
   * Computes 4 lists:
   *  - `sourcesInside`: the list of all Source Items that are
   *    children of the current item (can be thread/group/directory).
   *    This include any nested level of children.
   *  - `sourcesOutside`: all other Source Items.
   *    i.e. all sources that are in any other folder of any group/thread.
   *  - `allInsideBlackBoxed`, all sources of `sourcesInside` which are currently
   *    blackboxed.
   *  - `allOutsideBlackBoxed`, all sources of `sourcesOutside` which are currently
   *    blackboxed.
   */
  getBlackBoxSourcesGroups = item => {
    const allSources = [];
    function collectAllSources(list, _item) {
      if (_item.children) {
        _item.children.forEach(i => collectAllSources(list, i));
      }
      if (_item.type == "source") {
        list.push(_item.source);
      }
    }
    for (const rootItem of this.props.rootItems) {
      collectAllSources(allSources, rootItem);
    }

    const sourcesInside = [];
    collectAllSources(sourcesInside, item);

    const sourcesOutside = allSources.filter(
      source => !sourcesInside.includes(source)
    );
    const allInsideBlackBoxed = sourcesInside.every(
      source => this.props.blackBoxRanges[source.url]
    );
    const allOutsideBlackBoxed = sourcesOutside.every(
      source => this.props.blackBoxRanges[source.url]
    );

    return {
      sourcesInside,
      sourcesOutside,
      allInsideBlackBoxed,
      allOutsideBlackBoxed,
    };
  };

  renderItem = (item, depth, focused, _, expanded, { setExpanded }) => {
    const { mainThreadHost, projectRoot } = this.props;
    const isSourceBlackBoxed = item.source
      ? this.props.blackBoxRanges[item.source.url]
      : null;

    return (
      <SourcesTreeItem
        item={item}
        depth={depth}
        focused={focused}
        autoExpand={shouldAutoExpand(item, mainThreadHost)}
        expanded={expanded}
        focusItem={this.onFocus}
        selectSourceItem={this.selectSourceItem}
        isSourceBlackBoxed={isSourceBlackBoxed}
        projectRoot={projectRoot}
        setExpanded={setExpanded}
        getBlackBoxSourcesGroups={this.getBlackBoxSourcesGroups}
        getParent={this.getParent}
      />
    );
  };

  renderTree() {
    const { expanded, focused } = this.props;

    const { highlightItems } = this.state;

    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: 1,
      expanded,
      focused,
      getChildren: this.getChildren,
      getParent: this.getParent,
      getPath: this.getPath,
      getRoots: this.getRoots,
      highlightItems,
      itemHeight: 21,
      key: this.isEmpty() ? "empty" : "full",
      onCollapse: this.onCollapse,
      onExpand: this.onExpand,
      onFocus: this.onFocus,
      onActivate: this.onActivate,
      renderItem: this.renderItem,
      preventBlur: true,
    };

    return <ManagedTree {...treeProps} />;
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

  render() {
    return this.renderPane(
      this.isEmpty() ? (
        this.renderEmptyElement(L10N.getStr("noSourcesText"))
      ) : (
        <div key="tree" className="sources-list">
          {this.renderTree()}
        </div>
      )
    );
  }
}

function getSourceForTree(state, source) {
  if (!source) {
    return null;
  }

  if (!source.isPrettyPrinted) {
    return source;
  }

  return getGeneratedSourceByURL(state, getRawSourceURL(source.url));
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const rootItems = getSourcesTreeSources(state);

  return {
    cx: getContext(state),
    selectedSource: getSourceForTree(state, selectedSource),
    mainThreadHost: getMainThreadHost(state),
    expanded: getExpandedState(state),
    focused: getFocusedSourceItem(state),
    projectRoot: getProjectDirectoryRoot(state),
    rootItems,
    blackBoxRanges: getBlackBoxRanges(state),
  };
};

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  setExpandedState: actions.setExpandedState,
  focusItem: actions.focusItem,
})(SourcesTree);
