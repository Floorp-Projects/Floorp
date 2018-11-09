/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// Dependencies
import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "react-redux";

// Selectors
import {
  getShownSource,
  getSelectedSource,
  getDebuggeeUrl,
  getExpandedState,
  getProjectDirectoryRoot,
  getRelativeSources,
  getSourceCount
} from "../../selectors";

import { getGeneratedSourceByURL } from "../../reducers/sources";

// Actions
import actions from "../../actions";

// Components
import SourcesTreeItem from "./SourcesTreeItem";
import ManagedTree from "../shared/ManagedTree";
import Svg from "../shared/Svg";

// Utils
import {
  createTree,
  getDirectories,
  isDirectory,
  getSourceFromNode,
  nodeHasChildren,
  updateTree
} from "../../utils/sources-tree";
import { getRawSourceURL } from "../../utils/source";

import type {
  TreeNode,
  TreeDirectory,
  ParentMap
} from "../../utils/sources-tree/types";
import type { Source } from "../../types";
import type { SourcesMap, State as AppState } from "../../reducers/types";
import type { Item } from "../shared/ManagedTree";

type Props = {
  sources: SourcesMap,
  sourceCount: number,
  shownSource?: Source,
  selectedSource?: Source,
  debuggeeUrl: string,
  projectRoot: string,
  expanded: Set<string> | null,
  selectSource: string => mixed,
  setExpandedState: (Set<string>) => mixed,
  clearProjectDirectoryRoot: () => void
};

type State = {
  focusedItem: ?TreeNode,
  parentMap: ParentMap,
  sourceTree: TreeDirectory,
  uncollapsedTree: TreeDirectory,
  listItems?: any,
  highlightItems?: any
};

type SetExpanded = (item: TreeNode, expanded: boolean, altKey: boolean) => void;

class SourcesTree extends Component<Props, State> {
  mounted: boolean;

  constructor(props: Props) {
    super(props);
    const { debuggeeUrl, sources, projectRoot } = this.props;

    this.state = createTree({
      projectRoot,
      debuggeeUrl,
      sources
    });
  }

  componentWillReceiveProps(nextProps: Props) {
    const {
      projectRoot,
      debuggeeUrl,
      sources,
      shownSource,
      selectedSource
    } = this.props;
    const { uncollapsedTree, sourceTree } = this.state;

    if (
      projectRoot != nextProps.projectRoot ||
      debuggeeUrl != nextProps.debuggeeUrl ||
      nextProps.sourceCount === 0
    ) {
      // early recreate tree because of changes
      // to project root, debugee url or lack of sources
      return this.setState(
        createTree({
          sources: nextProps.sources,
          debuggeeUrl: nextProps.debuggeeUrl,
          projectRoot: nextProps.projectRoot
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
          prevSources: sources,
          debuggeeUrl,
          projectRoot,
          uncollapsedTree,
          sourceTree,
          focusedItem: this.state.focusedItem
        })
      );
    }
  }

  focusItem = (item: TreeNode) => {
    this.setState({ focusedItem: item });
  };

  selectItem = (item: TreeNode) => {
    if (item.type == "source" && !Array.isArray(item.contents)) {
      this.props.selectSource(item.contents.id);
    }
  };

  // NOTE: we get the source from sources because item.contents is cached
  getSource(item: TreeNode): ?Source {
    const source = getSourceFromNode(item);
    if (source) {
      return this.props.sources[source.id];
    }

    return null;
  }

  getPath = (item: TreeNode): string => {
    const path = `${item.path}/${item.name}`;
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

  onKeyDown = (e: KeyboardEvent) => {
    const { focusedItem } = this.state;

    if (e.keyCode === 13 && focusedItem) {
      this.selectItem(focusedItem);
    }
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

  renderProjectRootHeader() {
    const { projectRoot } = this.props;

    if (!projectRoot) {
      return null;
    }

    const rootLabel = projectRoot.split("/").pop();

    return (
      <div key="root" className="sources-clear-root-container">
        <button
          className="sources-clear-root"
          onClick={() => this.props.clearProjectDirectoryRoot()}
          title={L10N.getStr("removeDirectoryRoot.label")}
        >
          <Svg name="home" />
          <Svg name="breadcrumb" />
          <span className="sources-clear-root-label">{rootLabel}</span>
        </button>
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
    if (projectRoot) {
      if (sourceContents && sourceContents.name !== rootLabel) {
        return sourceContents.contents[0].contents;
      }
      return sourceContents.contents;
    }

    return sourceTree.contents;
  };

  renderItem = (
    item: TreeNode,
    depth: number,
    focused: boolean,
    _,
    expanded: boolean,
    { setExpanded }: { setExpanded: SetExpanded }
  ) => {
    const { debuggeeUrl, projectRoot } = this.props;

    return (
      <SourcesTreeItem
        item={item}
        depth={depth}
        focused={focused}
        expanded={expanded}
        focusItem={this.focusItem}
        selectItem={this.selectItem}
        source={this.getSource(item)}
        debuggeeUrl={debuggeeUrl}
        projectRoot={projectRoot}
        setExpanded={setExpanded}
      />
    );
  };

  renderTree() {
    const { expanded } = this.props;
    const { highlightItems, listItems, parentMap } = this.state;

    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: expanded ? 0 : 1,
      expanded,
      getChildren: (item: $Shape<TreeDirectory>) =>
        nodeHasChildren(item) ? item.contents : [],
      getParent: (item: $Shape<TreeNode>) => parentMap.get(item),
      getPath: this.getPath,
      getRoots: this.getRoots,
      highlightItems,
      itemHeight: 21,
      key: this.isEmpty() ? "empty" : "full",
      listItems,
      onCollapse: this.onCollapse,
      onExpand: this.onExpand,
      onFocus: this.focusItem,
      renderItem: this.renderItem,
      preventBlur: true
    };

    return <ManagedTree {...treeProps} />;
  }

  renderPane(...children) {
    const { projectRoot } = this.props;

    return (
      <div
        key="pane"
        className={classnames("sources-pane", {
          "sources-list-custom-root": projectRoot
        })}
      >
        {children}
      </div>
    );
  }

  render() {
    const { projectRoot } = this.props;

    if (this.isEmpty()) {
      if (projectRoot) {
        return this.renderPane(
          this.renderProjectRootHeader(),
          this.renderEmptyElement(L10N.getStr("sources.noSourcesAvailableRoot"))
        );
      }

      return this.renderPane(
        this.renderEmptyElement(L10N.getStr("sources.noSourcesAvailable"))
      );
    }

    return this.renderPane(
      this.renderProjectRootHeader(),
      <div key="tree" className="sources-list" onKeyDown={this.onKeyDown}>
        {this.renderTree()}
      </div>
    );
  }
}

function getSourceForTree(state: AppState, source: ?Source): ?Source | null {
  if (!source || !source.isPrettyPrinted) {
    return source;
  }

  return getGeneratedSourceByURL(state, getRawSourceURL(source.url));
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const shownSource = getShownSource(state);

  return {
    shownSource: getSourceForTree(state, shownSource),
    selectedSource: getSourceForTree(state, selectedSource),
    debuggeeUrl: getDebuggeeUrl(state),
    expanded: getExpandedState(state),
    projectRoot: getProjectDirectoryRoot(state),
    sources: getRelativeSources(state),
    sourceCount: getSourceCount(state)
  };
};

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    setExpandedState: actions.setExpandedState,
    clearProjectDirectoryRoot: actions.clearProjectDirectoryRoot
  }
)(SourcesTree);
