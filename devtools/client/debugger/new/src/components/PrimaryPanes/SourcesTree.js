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
  getDisplayedSourcesForThread,
  getFocusedSourceItem,
  getWorkerByThread,
  getWorkerCount
} from "../../selectors";

import { getGeneratedSourceByURL } from "../../reducers/sources";

// Actions
import actions from "../../actions";

// Components
import AccessibleImage from "../shared/AccessibleImage";
import SourcesTreeItem from "./SourcesTreeItem";
import ManagedTree from "../shared/ManagedTree";

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

import { getDisplayName } from "../../utils/workers";
import { features } from "../../utils/prefs";

import type {
  TreeNode,
  TreeDirectory,
  ParentMap
} from "../../utils/sources-tree/types";
import type { Worker, Source } from "../../types";
import type { SourcesMap, State as AppState } from "../../reducers/types";
import type { Item } from "../shared/ManagedTree";

type Props = {
  thread: string,
  worker: Worker,
  sources: SourcesMap,
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
  workerCount: number
};

type State = {
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
          sourceTree
        })
      );
    }
  }

  selectItem = (item: TreeNode) => {
    if (item.type == "source" && !Array.isArray(item.contents)) {
      this.props.selectSource(item.contents.id);
    }
  };

  onFocus = (item: TreeNode) => {
    this.props.focusItem({ thread: this.props.thread, item });
  };

  onActivate = (item: TreeNode) => {
    this.selectItem(item);
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
    this.props.setExpandedState(this.props.thread, expandedState);
  };

  onCollapse = (item: Item, expandedState: Set<string>) => {
    this.props.setExpandedState(this.props.thread, expandedState);
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
      autoExpandDepth: expanded ? 0 : 1,
      expanded,
      focused,
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
      onFocus: this.onFocus,
      onActivate: this.onActivate,
      renderItem: this.renderItem,
      preventBlur: true
    };

    return <ManagedTree {...treeProps} />;
  }

  renderPane(...children) {
    const { projectRoot, thread } = this.props;

    return (
      <div
        key="pane"
        className={classnames("sources-pane", {
          "sources-list-custom-root": projectRoot,
          thread
        })}
      >
        {children}
      </div>
    );
  }

  renderThreadHeader() {
    const { worker, workerCount } = this.props;

    if (!features.windowlessWorkers || workerCount == 0) {
      return null;
    }

    if (worker) {
      return (
        <div className="node thread-header">
          <AccessibleImage className="worker" />
          <span className="label">{getDisplayName(worker)}</span>
        </div>
      );
    }

    return (
      <div className="node thread-header">
        <AccessibleImage className={"file"} />
        <span className="label">{L10N.getStr("mainThread")}</span>
      </div>
    );
  }

  render() {
    const { worker } = this.props;

    if (!features.windowlessWorkers && worker) {
      return null;
    }

    return this.renderPane(
      this.renderThreadHeader(),
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
  displayedSources: SourcesMap,
  source: ?Source,
  thread
): ?Source {
  if (!source) {
    return null;
  }

  source = displayedSources[source.id];
  if (!source || !source.isPrettyPrinted) {
    return source;
  }

  return getGeneratedSourceByURL(state, getRawSourceURL(source.url));
}

const mapStateToProps = (state, props) => {
  const selectedSource = getSelectedSource(state);
  const shownSource = getShownSource(state);
  const focused = getFocusedSourceItem(state);
  const thread = props.thread;
  const displayedSources = getDisplayedSourcesForThread(state, thread);

  return {
    shownSource: getSourceForTree(state, displayedSources, shownSource, thread),
    selectedSource: getSourceForTree(
      state,
      displayedSources,
      selectedSource,
      thread
    ),
    debuggeeUrl: getDebuggeeUrl(state),
    expanded: getExpandedState(state, props.thread),
    focused: focused && focused.thread == props.thread ? focused.item : null,
    projectRoot: getProjectDirectoryRoot(state),
    sources: displayedSources,
    sourceCount: Object.values(displayedSources).length,
    worker: getWorkerByThread(state, thread),
    workerCount: getWorkerCount(state)
  };
};

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    setExpandedState: actions.setExpandedState,
    focusItem: actions.focusItem
  }
)(SourcesTree);
