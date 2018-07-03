"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _selectors = require("../../selectors/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _source = require("../../utils/source");

var _SourcesTreeItem = require("./SourcesTreeItem");

var _SourcesTreeItem2 = _interopRequireDefault(_SourcesTreeItem);

var _ManagedTree = require("../shared/ManagedTree");

var _ManagedTree2 = _interopRequireDefault(_ManagedTree);

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _sourcesTree = require("../../utils/sources-tree/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Dependencies
// Selectors
// Actions
// Components
// Utils
class SourcesTree extends _react.Component {
  constructor(props) {
    super(props);

    _initialiseProps.call(this);

    const {
      debuggeeUrl,
      sources,
      projectRoot
    } = this.props;
    this.state = (0, _sourcesTree.createTree)({
      projectRoot,
      debuggeeUrl,
      sources
    });
  }

  componentWillReceiveProps(nextProps) {
    const {
      projectRoot,
      debuggeeUrl,
      sources,
      shownSource,
      selectedSource
    } = this.props;
    const {
      uncollapsedTree,
      sourceTree
    } = this.state;

    if (projectRoot != nextProps.projectRoot || debuggeeUrl != nextProps.debuggeeUrl || nextProps.sourceCount === 0) {
      // early recreate tree because of changes
      // to project root, debugee url or lack of sources
      return this.setState((0, _sourcesTree.createTree)({
        sources: nextProps.sources,
        debuggeeUrl: nextProps.debuggeeUrl,
        projectRoot: nextProps.projectRoot
      }));
    }

    if (nextProps.shownSource && nextProps.shownSource != shownSource) {
      const matchingSources = Object.keys(sources).filter(sourceId => {
        return (0, _source.getRawSourceURL)(sources[sourceId].url) === nextProps.shownSource;
      });

      if (matchingSources.length) {
        const listItems = (0, _sourcesTree.getDirectories)(sources[matchingSources[0]], sourceTree);

        if (listItems && listItems.length) {
          this.selectItem(listItems[0]);
        }

        return this.setState({
          listItems
        });
      }
    }

    if (nextProps.selectedSource && nextProps.selectedSource != selectedSource) {
      const highlightItems = (0, _sourcesTree.getDirectories)(nextProps.selectedSource, sourceTree);
      this.setState({
        highlightItems
      });
    } // NOTE: do not run this every time a source is clicked,
    // only when a new source is added


    if (nextProps.sources != this.props.sources) {
      this.setState((0, _sourcesTree.updateTree)({
        newSources: nextProps.sources,
        prevSources: sources,
        debuggeeUrl,
        projectRoot,
        uncollapsedTree,
        sourceTree
      }));
    }
  }

  // NOTE: we get the source from sources because item.contents is cached
  getSource(item) {
    const source = (0, _sourcesTree.getSourceFromNode)(item);

    if (source) {
      return this.props.sources[source.id];
    }

    return null;
  }

  isEmpty() {
    const {
      sourceTree
    } = this.state;
    return sourceTree.contents.length === 0;
  }

  renderEmptyElement(message) {
    return _react2.default.createElement("div", {
      key: "empty",
      className: "no-sources-message"
    }, message);
  }

  renderProjectRootHeader() {
    const {
      projectRoot
    } = this.props;

    if (!projectRoot) {
      return null;
    }

    const rootLabel = projectRoot.split("/").pop();
    return _react2.default.createElement("div", {
      key: "root",
      className: "sources-clear-root-container"
    }, _react2.default.createElement("button", {
      className: "sources-clear-root",
      onClick: () => this.props.clearProjectDirectoryRoot(),
      title: L10N.getStr("removeDirectoryRoot.label")
    }, _react2.default.createElement(_Svg2.default, {
      name: "home"
    }), _react2.default.createElement(_Svg2.default, {
      name: "breadcrumb",
      "class": true
    }), _react2.default.createElement("span", {
      className: "sources-clear-root-label"
    }, rootLabel)));
  }

  renderTree() {
    const {
      expanded
    } = this.props;
    const {
      highlightItems,
      listItems,
      parentMap
    } = this.state;
    const treeProps = {
      autoExpandAll: false,
      autoExpandDepth: expanded ? 0 : 1,
      expanded,
      getChildren: item => (0, _sourcesTree.nodeHasChildren)(item) ? item.contents : [],
      getParent: item => parentMap.get(item),
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
    return _react2.default.createElement(_ManagedTree2.default, treeProps);
  }

  renderPane(...children) {
    const {
      projectRoot
    } = this.props;
    return _react2.default.createElement("div", {
      key: "pane",
      className: (0, _classnames2.default)("sources-pane", {
        "sources-list-custom-root": projectRoot
      })
    }, children);
  }

  render() {
    const {
      projectRoot
    } = this.props;

    if (this.isEmpty()) {
      if (projectRoot) {
        return this.renderPane(this.renderProjectRootHeader(), this.renderEmptyElement(L10N.getStr("sources.noSourcesAvailableRoot")));
      }

      return this.renderPane(this.renderEmptyElement(L10N.getStr("sources.noSourcesAvailable")));
    }

    return this.renderPane(this.renderProjectRootHeader(), _react2.default.createElement("div", {
      key: "tree",
      className: "sources-list",
      onKeyDown: this.onKeyDown
    }, this.renderTree()));
  }

}

var _initialiseProps = function () {
  this.focusItem = item => {
    this.setState({
      focusedItem: item
    });
  };

  this.selectItem = item => {
    if (!(0, _sourcesTree.isDirectory)(item) && // This second check isn't strictly necessary, but it ensures that Flow
    // knows that we are doing the correct thing.
    !Array.isArray(item.contents)) {
      this.props.selectSource(item.contents.id);
    }
  };

  this.getPath = item => {
    const path = `${item.path}/${item.name}`;

    if ((0, _sourcesTree.isDirectory)(item)) {
      return path;
    }

    const source = this.getSource(item);
    const blackBoxedPart = source && source.isBlackBoxed ? ":blackboxed" : ""; // Original and generated sources can point to the same path
    // therefore necessary to distinguish as path is used as keys.

    const generatedPart = source && source.sourceMapURL ? ":generated" : "";
    return `${path}${blackBoxedPart}${generatedPart}`;
  };

  this.onExpand = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
  };

  this.onCollapse = (item, expandedState) => {
    this.props.setExpandedState(expandedState);
  };

  this.onKeyDown = e => {
    const {
      focusedItem
    } = this.state;

    if (e.keyCode === 13 && focusedItem) {
      this.selectItem(focusedItem);
    }
  };

  this.getRoots = () => {
    const {
      projectRoot
    } = this.props;
    const {
      sourceTree
    } = this.state;
    const sourceContents = sourceTree.contents[0];
    const rootLabel = projectRoot.split("/").pop(); // The "sourceTree.contents[0]" check ensures that there are contents
    // A custom root with no existing sources will be ignored

    if (projectRoot) {
      if (sourceContents && sourceContents.name !== rootLabel) {
        return sourceContents.contents[0].contents;
      }

      return sourceContents.contents;
    }

    return sourceTree.contents;
  };

  this.renderItem = (item, depth, focused, _, expanded, {
    setExpanded
  }) => {
    const {
      debuggeeUrl,
      projectRoot
    } = this.props;
    return _react2.default.createElement(_SourcesTreeItem2.default, {
      item: item,
      depth: depth,
      focused: focused,
      expanded: expanded,
      setExpanded: setExpanded,
      focusItem: this.focusItem,
      selectItem: this.selectItem,
      source: this.getSource(item),
      debuggeeUrl: debuggeeUrl,
      projectRoot: projectRoot,
      clearProjectDirectoryRoot: this.props.clearProjectDirectoryRoot,
      setProjectDirectoryRoot: this.props.setProjectDirectoryRoot
    });
  };
};

const mapStateToProps = state => {
  return {
    shownSource: (0, _selectors.getShownSource)(state),
    selectedSource: (0, _selectors.getSelectedSource)(state),
    debuggeeUrl: (0, _selectors.getDebuggeeUrl)(state),
    expanded: (0, _selectors.getExpandedState)(state),
    projectRoot: (0, _selectors.getProjectDirectoryRoot)(state),
    sources: (0, _selectors.getRelativeSources)(state),
    sourceCount: (0, _selectors.getSourceCount)(state)
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  selectSource: _actions2.default.selectSource,
  setExpandedState: _actions2.default.setExpandedState,
  setProjectDirectoryRoot: _actions2.default.setProjectDirectoryRoot,
  clearProjectDirectoryRoot: _actions2.default.clearProjectDirectoryRoot
})(SourcesTree);