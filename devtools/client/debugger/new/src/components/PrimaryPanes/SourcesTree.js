"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _SourceIcon = require("../shared/SourceIcon");

var _SourceIcon2 = _interopRequireDefault(_SourceIcon);

var _selectors = require("../../selectors/index");

var _sourceTree = require("../../actions/source-tree");

var _sources = require("../../actions/sources/index");

var _ui = require("../../actions/ui");

var _ManagedTree = require("../shared/ManagedTree");

var _ManagedTree2 = _interopRequireDefault(_ManagedTree);

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _sourcesTree = require("../../utils/sources-tree/index");

var _source = require("../../utils/source");

var _clipboard = require("../../utils/clipboard");

var _prefs = require("../../utils/prefs");

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
      const listItems = (0, _sourcesTree.getDirectories)(nextProps.shownSource, sourceTree);

      if (listItems && listItems[0]) {
        this.selectItem(listItems[0]);
      }

      return this.setState({
        listItems
      });
    }

    if (nextProps.selectedSource && nextProps.selectedSource != selectedSource) {
      const highlightItems = (0, _sourcesTree.getDirectories)((0, _source.getRawSourceURL)(nextProps.selectedSource.url), sourceTree);
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

  renderItemName(name) {
    switch (name) {
      case "ng://":
        return "Angular";

      case "webpack://":
        return "Webpack";

      case "moz-extension://":
        return L10N.getStr("extensionsText");

      default:
        return name;
    }
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
    const blackBoxedPart = source && source.isBlackBoxed ? ":blackboxed" : "";
    return `${path}${blackBoxedPart}`;
  };

  this.getIcon = (sources, item, depth) => {
    const {
      debuggeeUrl,
      projectRoot
    } = this.props;

    if (item.path === "webpack://") {
      return _react2.default.createElement(_Svg2.default, {
        name: "webpack"
      });
    } else if (item.path === "ng://") {
      return _react2.default.createElement(_Svg2.default, {
        name: "angular"
      });
    } else if (item.path === "moz-extension://") {
      return _react2.default.createElement("img", {
        className: "extension"
      });
    }

    if (depth === 0 && projectRoot === "") {
      return _react2.default.createElement("img", {
        className: (0, _classnames2.default)("domain", {
          debuggee: debuggeeUrl && debuggeeUrl.includes(item.name)
        })
      });
    }

    if ((0, _sourcesTree.isDirectory)(item)) {
      return _react2.default.createElement("img", {
        className: "folder"
      });
    }

    const source = this.getSource(item);

    if (source) {
      return _react2.default.createElement(_SourceIcon2.default, {
        source: source
      });
    }

    return null;
  };

  this.onContextMenu = (event, item) => {
    const copySourceUri2Label = L10N.getStr("copySourceUri2");
    const copySourceUri2Key = L10N.getStr("copySourceUri2.accesskey");
    const setDirectoryRootLabel = L10N.getStr("setDirectoryRoot.label");
    const setDirectoryRootKey = L10N.getStr("setDirectoryRoot.accesskey");
    const removeDirectoryRootLabel = L10N.getStr("removeDirectoryRoot.label");
    event.stopPropagation();
    event.preventDefault();
    const menuOptions = [];

    if (!(0, _sourcesTree.isDirectory)(item)) {
      // Flow requires some extra handling to ensure the value of contents.
      const {
        contents
      } = item;

      if (!Array.isArray(contents)) {
        const copySourceUri2 = {
          id: "node-menu-copy-source",
          label: copySourceUri2Label,
          accesskey: copySourceUri2Key,
          disabled: false,
          click: () => (0, _clipboard.copyToTheClipboard)(contents.url)
        };
        menuOptions.push(copySourceUri2);
      }
    }

    if ((0, _sourcesTree.isDirectory)(item) && _prefs.features.root) {
      const {
        path
      } = item;
      const {
        projectRoot
      } = this.props;

      if (projectRoot.endsWith(path)) {
        menuOptions.push({
          id: "node-remove-directory-root",
          label: removeDirectoryRootLabel,
          disabled: false,
          click: () => this.props.clearProjectDirectoryRoot()
        });
      } else {
        menuOptions.push({
          id: "node-set-directory-root",
          label: setDirectoryRootLabel,
          accesskey: setDirectoryRootKey,
          disabled: false,
          click: () => this.props.setProjectDirectoryRoot(path)
        });
      }
    }

    (0, _devtoolsContextmenu.showMenu)(event, menuOptions);
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

  this.renderItem = (item, depth, focused, _, expanded, {
    setExpanded
  }) => {
    const arrow = (0, _sourcesTree.isDirectory)(item) ? _react2.default.createElement("img", {
      className: (0, _classnames2.default)("arrow", {
        expanded: expanded
      })
    }) : _react2.default.createElement("i", {
      className: "no-arrow"
    });
    const {
      sources
    } = this.props;
    const icon = this.getIcon(sources, item, depth);
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("node", {
        focused
      }),
      key: item.path,
      onClick: e => {
        this.focusItem(item);

        if ((0, _sourcesTree.isDirectory)(item)) {
          setExpanded(item, !!expanded, e.altKey);
        } else {
          this.selectItem(item);
        }
      },
      onContextMenu: e => this.onContextMenu(e, item)
    }, arrow, icon, _react2.default.createElement("span", {
      className: "label"
    }, " ", this.renderItemName(item.name), " "));
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

const actionCreators = {
  setExpandedState: _sourceTree.setExpandedState,
  selectSource: _sources.selectSource,
  setProjectDirectoryRoot: _ui.setProjectDirectoryRoot,
  clearProjectDirectoryRoot: _ui.clearProjectDirectoryRoot
};
exports.default = (0, _reactRedux.connect)(mapStateToProps, actionCreators)(SourcesTree);