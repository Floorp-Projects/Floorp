/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "../../utils/connect";
import { difference } from "lodash";

// Selectors
import {
  getShownSource,
  getSelectedSource,
  getDebuggeeUrl,
  getExpandedState,
  getProjectDirectoryRoot,
  getDisplayedSources,
  getFocusedSourceItem,
  getContext,
} from "../../selectors";

import { getGeneratedSourceByURL } from "../../reducers/sources";

// Actions
import actions from "../../actions";

// Components
import SourcesTreeItem from "./SourcesTreeItem";
import ManagedTree from "../shared/ManagedTree";

// Utils
import {
  createTree,
  getDirectories,
  isDirectory,
  findSourceTreeNodes,
  updateTree,
  getSource,
  getChildren,
  getAllSources,
  getSourcesInsideGroup,
} from "../../utils/sources-tree";
import { parse } from "../../utils/url";
import { getRawSourceURL } from "../../utils/source";

function shouldAutoExpand(depth, item, debuggeeUrl, projectRoot) {
  if (projectRoot != "" || depth !== 1) {
    return false;
  }

  const { host } = parse(debuggeeUrl);
  return item.name === host;
}

class SourcesTree extends Component {
  constructor(props) {
    super(props);
    const { debuggeeUrl, sources, threads } = this.props;

    this.state = createTree({
      debuggeeUrl,
      sources,
      threads,
    });
  }

  componentWillReceiveProps(nextProps) {
    const {
      projectRoot,
      debuggeeUrl,
      sources,
      shownSource,
      selectedSource,
      threads,
    } = this.props;
    const { uncollapsedTree, sourceTree } = this.state;

    if (
      projectRoot != nextProps.projectRoot ||
      debuggeeUrl != nextProps.debuggeeUrl ||
      threads != nextProps.threads ||
      nextProps.sourceCount === 0
    ) {
      // early recreate tree because of changes
      // to project root, debuggee url or lack of sources
      return this.setState(
        createTree({
          sources: nextProps.sources,
          debuggeeUrl: nextProps.debuggeeUrl,
          threads: nextProps.threads,
        })
      );
    }

    if (nextProps.shownSource && nextProps.shownSource != shownSource) {
      const listItems = getDirectories(nextProps.shownSource, sourceTree);
      return this.setState({ listItems });
    }

    if (
      nextProps.selectedSource &&
      nextProps.selectedSource != selectedSource
    ) {
      const highlightItems = getDirectories(
        nextProps.selectedSource,
        sourceTree
      );
      this.setState({ highlightItems });
    }

    // NOTE: do not run this every time a source is clicked,
    // only when a new source is added
    if (nextProps.sources != this.props.sources) {
      const update = updateTree({
        newSources: nextProps.sources,
        threads: nextProps.threads,
        prevSources: sources,
        debuggeeUrl,
        uncollapsedTree,
        sourceTree,
      });
      if (update) {
        this.setState(update);
      }
    }
  }

  selectItem = item => {
    if (item.type == "source" && !Array.isArray(item.contents)) {
      this.props.selectSource(this.props.cx, item.contents.id);
    }
  };

  onFocus = item => {
    this.props.focusItem(item);
  };

  onActivate = item => {
    this.selectItem(item);
  };

  getPath = item => {
    const { path } = item;
    const source = getSource(item, this.props);

    if (!source || isDirectory(item)) {
      return path;
    }

    const blackBoxedPart = source.isBlackBoxed ? ":blackboxed" : "";

    return `${path}/${source.id}/${blackBoxedPart}`;
  };

  onExpand = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
  };

  onCollapse = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
  };

  isEmpty() {
    const { sourceTree } = this.state;
    return sourceTree.contents.length === 0;
  }

  renderEmptyElement(message) {
    return (
      <div key="empty" className="no-sources-message">
        {message}
      </div>
    );
  }

  getRoots = (sourceTree, projectRoot) => {
    const sourceContents = sourceTree.contents[0];

    if (projectRoot && sourceContents) {
      const roots = findSourceTreeNodes(sourceTree, projectRoot);
      // NOTE if there is one root, we want to show its content
      // TODO with multiple roots we should try and show the thread name
      return roots && roots.length == 1 ? roots[0].contents : roots;
    }

    return sourceTree.contents;
  };

  getSourcesGroups = item => {
    const sourcesAll = getAllSources(this.props);
    const sourcesInside = getSourcesInsideGroup(item, this.props);
    const sourcesOuside = difference(sourcesAll, sourcesInside);

    return { sourcesInside, sourcesOuside };
  };

  renderItem = (item, depth, focused, _, expanded, { setExpanded }) => {
    const { debuggeeUrl, projectRoot, threads } = this.props;

    return (
      <SourcesTreeItem
        item={item}
        threads={threads}
        depth={depth}
        focused={focused}
        autoExpand={shouldAutoExpand(depth, item, debuggeeUrl, projectRoot)}
        expanded={expanded}
        focusItem={this.onFocus}
        selectItem={this.selectItem}
        source={getSource(item, this.props)}
        debuggeeUrl={debuggeeUrl}
        projectRoot={projectRoot}
        setExpanded={setExpanded}
        getSourcesGroups={this.getSourcesGroups}
      />
    );
  };

  renderTree() {
    const { expanded, focused, projectRoot } = this.props;

    const { highlightItems, listItems, parentMap, sourceTree } = this.state;

    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: 1,
      expanded,
      focused,
      getChildren: getChildren,
      getParent: item => parentMap.get(item),
      getPath: this.getPath,
      getRoots: () => this.getRoots(sourceTree, projectRoot),
      highlightItems,
      itemHeight: 21,
      key: this.isEmpty() ? "empty" : "full",
      listItems,
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
          "sources-list-custom-root": projectRoot,
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

function getSourceForTree(state, displayedSources, source) {
  if (!source) {
    return null;
  }

  if (!source || !source.isPrettyPrinted) {
    return source;
  }

  return getGeneratedSourceByURL(state, getRawSourceURL(source.url));
}

const mapStateToProps = (state, props) => {
  const selectedSource = getSelectedSource(state);
  const shownSource = getShownSource(state);
  const displayedSources = getDisplayedSources(state);

  return {
    threads: props.threads,
    cx: getContext(state),
    shownSource: getSourceForTree(state, displayedSources, shownSource),
    selectedSource: getSourceForTree(state, displayedSources, selectedSource),
    debuggeeUrl: getDebuggeeUrl(state),
    expanded: getExpandedState(state),
    focused: getFocusedSourceItem(state),
    projectRoot: getProjectDirectoryRoot(state),
    sources: displayedSources,
    sourceCount: Object.values(displayedSources).length,
  };
};

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  setExpandedState: actions.setExpandedState,
  focusItem: actions.focusItem,
})(SourcesTree);
