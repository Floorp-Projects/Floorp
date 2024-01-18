/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
import React, {
  Component,
  Fragment,
} from "devtools/client/shared/vendor/react";
import {
  div,
  button,
  span,
  footer,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";

// Selectors
import {
  getMainThreadHost,
  getExpandedState,
  getProjectDirectoryRoot,
  getProjectDirectoryRootName,
  getSourcesTreeSources,
  getFocusedSourceItem,
  getHideIgnoredSources,
} from "../../selectors/index";

// Actions
import actions from "../../actions/index";

// Components
import SourcesTreeItem from "./SourcesTreeItem";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");
const Tree = require("resource://devtools/client/shared/components/Tree.js");

function shouldAutoExpand(item, mainThreadHost) {
  // There is only one case where we want to force auto expand,
  // when we are on the group of the page's domain.
  return item.type == "group" && item.groupName === mainThreadHost;
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
      setExpandedState: PropTypes.func.isRequired,
      rootItems: PropTypes.object.isRequired,
      clearProjectDirectoryRoot: PropTypes.func.isRequired,
      projectRootName: PropTypes.string.isRequired,
      setHideOrShowIgnoredSources: PropTypes.func.isRequired,
      hideIgnoredSources: PropTypes.bool.isRequired,
    };
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
    // Note that setExpandedState relies on us to clone this Set
    // which is going to be store as-is in the reducer.
    const expanded = new Set(this.props.expanded);

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
    return div(
      {
        key: "empty",
        className: "no-sources-message",
      },
      message
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
    return div(
      {
        key: "root",
        className: "sources-clear-root-container",
      },
      button(
        {
          className: "sources-clear-root",
          onClick: () => this.props.clearProjectDirectoryRoot(),
          title: L10N.getStr("removeDirectoryRoot.label"),
        },
        React.createElement(AccessibleImage, {
          className: "home",
        }),
        React.createElement(AccessibleImage, {
          className: "breadcrumb",
        }),
        span(
          {
            className: "sources-clear-root-label",
          },
          projectRootName
        )
      )
    );
  }

  renderItem = (item, depth, focused, _, expanded) => {
    const { mainThreadHost } = this.props;
    return React.createElement(SourcesTreeItem, {
      item,
      depth,
      focused,
      autoExpand: shouldAutoExpand(item, mainThreadHost),
      expanded,
      focusItem: this.onFocus,
      selectSourceItem: this.selectSourceItem,
      setExpanded: this.setExpanded,
      getParent: this.getParent,
    });
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
    return React.createElement(Tree, treeProps);
  }

  renderPane(child) {
    const { projectRoot } = this.props;
    return div(
      {
        key: "pane",
        className: classnames("sources-pane", {
          "sources-list-custom-root": !!projectRoot,
        }),
      },
      child
    );
  }

  renderFooter() {
    if (this.props.hideIgnoredSources) {
      return footer(
        {
          className: "source-list-footer",
        },
        L10N.getStr("ignoredSourcesHidden"),
        button(
          {
            className: "devtools-togglebutton",
            onClick: () => this.props.setHideOrShowIgnoredSources(false),
            title: L10N.getStr("showIgnoredSources.tooltip.label"),
          },
          L10N.getStr("showIgnoredSources")
        )
      );
    }
    return null;
  }

  render() {
    const { projectRoot } = this.props;
    return div(
      {
        key: "pane",
        className: classnames("sources-list", {
          "sources-list-custom-root": !!projectRoot,
        }),
      },
      this.isEmpty()
        ? this.renderEmptyElement(L10N.getStr("noSourcesText"))
        : React.createElement(
            Fragment,
            null,
            this.renderProjectRootHeader(),
            this.renderTree(),
            this.renderFooter()
          )
    );
  }
}

const mapStateToProps = state => {
  return {
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
