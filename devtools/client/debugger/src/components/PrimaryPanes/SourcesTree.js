/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// Dependencies
import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "../../utils/connect";

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
  getSourceFromNode,
  nodeHasChildren,
  updateTree,
} from "../../utils/sources-tree";
import { parse } from "../../utils/url";
import { getRawSourceURL } from "../../utils/source";

import type {
  TreeNode,
  TreeDirectory,
  ParentMap,
} from "../../utils/sources-tree/types";
import type { Source, Context, Thread } from "../../types";
import type {
  SourcesMapByThread,
  State as AppState,
} from "../../reducers/types";
import type { Item } from "../shared/ManagedTree";

type Props = {
  cx: Context,
  threads: Thread[],
  sources: SourcesMapByThread,
  sourceCount: number,
  shownSource?: Source,
  selectedSource?: Source,
  debuggeeUrl: string,
  projectRoot: string,
  expanded: Set<string>,
  selectSource: typeof actions.selectSource,
  setExpandedState: typeof actions.setExpandedState,
  focusItem: typeof actions.focusItem,
  focused: TreeNode,
};

type State = {
  parentMap: ParentMap,
  sourceTree: TreeDirectory,
  uncollapsedTree: TreeDirectory,
  listItems?: any,
  highlightItems?: any,
};

type SetExpanded = (item: TreeNode, expanded: boolean, altKey: boolean) => void;

function shouldAutoExpand(depth, item, debuggeeUrl) {
  if (depth !== 1) {
    return false;
  }

  const { host } = parse(debuggeeUrl);
  return item.name === host;
}

class SourcesTree extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    const { debuggeeUrl, sources, threads } = this.props;

    this.state = createTree({
      debuggeeUrl,
      sources,
      threads,
    });
  }

  componentWillReceiveProps(nextProps: Props) {
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
      this.setState(
        updateTree({
          newSources: nextProps.sources,
          threads: nextProps.threads,
          prevSources: sources,
          debuggeeUrl,
          uncollapsedTree,
          sourceTree,
        })
      );
    }
  }

  selectItem = (item: TreeNode) => {
    if (item.type == "source" && !Array.isArray(item.contents)) {
      this.props.selectSource(this.props.cx, item.contents.id);
    }
  };

  onFocus = (item: TreeNode) => {
    this.props.focusItem(item);
  };

  onActivate = (item: TreeNode) => {
    this.selectItem(item);
  };

  // NOTE: we get the source from sources because item.contents is cached
  getSource(item: TreeNode): ?Source {
    return getSourceFromNode(item);
  }

  getPath = (item: TreeNode): string => {
    const { path } = item;
    const source = this.getSource(item);

    if (!source || isDirectory(item)) {
      return path;
    }

    const blackBoxedPart = source.isBlackBoxed ? ":blackboxed" : "";

    return `${path}/${source.id}/${blackBoxedPart}`;
  };

  onExpand = (item: Item, expandedState: Set<string>) => {
    this.props.setExpandedState(expandedState);
  };

  onCollapse = (item: Item, expandedState: Set<string>) => {
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

  getRoots = () => {
    const { projectRoot } = this.props;
    const { sourceTree } = this.state;

    const sourceContents = sourceTree.contents[0];
    const rootLabel = projectRoot.split("/").pop();

    // The "sourceTree.contents[0]" check ensures that there are contents
    // A custom root with no existing sources will be ignored
    if (projectRoot && sourceContents) {
      if (sourceContents && sourceContents.name !== rootLabel) {
        return sourceContents.contents[0].contents;
      }
      return sourceContents.contents;
    }

    return sourceTree.contents;
  };

  getChildren = (item: $Shape<TreeDirectory>) => {
    return nodeHasChildren(item) ? item.contents : [];
  };

  renderItem = (
    item: TreeNode,
    depth: number,
    focused: boolean,
    _,
    expanded: boolean,
    { setExpanded }: { setExpanded: SetExpanded }
  ) => {
    const { debuggeeUrl, projectRoot, threads } = this.props;

    return (
      <SourcesTreeItem
        item={item}
        threads={threads}
        depth={depth}
        focused={focused}
        autoExpand={shouldAutoExpand(depth, item, debuggeeUrl)}
        expanded={expanded}
        focusItem={this.onFocus}
        selectItem={this.selectItem}
        source={this.getSource(item)}
        debuggeeUrl={debuggeeUrl}
        projectRoot={projectRoot}
        setExpanded={setExpanded}
      />
    );
  };

  renderTree() {
    const { expanded, focused } = this.props;

    const { highlightItems, listItems, parentMap } = this.state;

    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: 1,
      expanded,
      focused,
      getChildren: this.getChildren,
      getParent: (item: $Shape<TreeNode>) => parentMap.get(item),
      getPath: this.getPath,
      getRoots: this.getRoots,
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

  renderPane(...children) {
    const { projectRoot } = this.props;

    return (
      <div
        key="pane"
        className={classnames("sources-pane", {
          "sources-list-custom-root": projectRoot,
        })}
      >
        {children}
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

function getSourceForTree(
  state: AppState,
  displayedSources: SourcesMapByThread,
  source: ?Source
): ?Source {
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

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    setExpandedState: actions.setExpandedState,
    focusItem: actions.focusItem,
  }
)(SourcesTree);
