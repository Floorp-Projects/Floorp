"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ProjectSearch = undefined;

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _actions = require("../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _editor = require("../utils/editor/index");

var _projectSearch = require("../utils/project-search");

var _projectTextSearch = require("../reducers/project-text-search");

var _sourcesTree = require("../utils/sources-tree/index");

var _selectors = require("../selectors/index");

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _ManagedTree = require("./shared/ManagedTree");

var _ManagedTree2 = _interopRequireDefault(_ManagedTree);

var _SearchInput = require("./shared/SearchInput");

var _SearchInput2 = _interopRequireDefault(_SearchInput);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function getFilePath(item, index) {
  return item.type === "RESULT" ? `${item.sourceId}-${index || "$"}` : `${item.sourceId}-${item.line}-${item.column}-${index || "$"}`;
}

function sanitizeQuery(query) {
  // no '\' at end of query
  return query.replace(/\\$/, "");
}

class ProjectSearch extends _react.Component {
  constructor(props) {
    super(props);

    this.toggleProjectTextSearch = (key, e) => {
      const {
        closeProjectSearch,
        setActiveSearch
      } = this.props;

      if (e) {
        e.preventDefault();
      }

      if (this.isProjectSearchEnabled()) {
        return closeProjectSearch();
      }

      return setActiveSearch("project");
    };

    this.isProjectSearchEnabled = () => this.props.activeSearch === "project";

    this.selectMatchItem = matchItem => {
      this.props.selectLocation(_objectSpread({}, matchItem));
      this.props.doSearchForHighlight(this.state.inputValue, (0, _editor.getEditor)(), matchItem.line, matchItem.column);
    };

    this.getResults = () => {
      const {
        results
      } = this.props;
      return results.toJS().map(result => _objectSpread({
        type: "RESULT"
      }, result, {
        matches: result.matches.map(m => _objectSpread({
          type: "MATCH"
        }, m))
      })).filter(result => result.filepath && result.matches.length > 0);
    };

    this.getResultCount = () => this.getResults().reduce((count, file) => count + file.matches.length, 0);

    this.onKeyDown = e => {
      if (e.key === "Escape") {
        return;
      }

      e.stopPropagation();

      if (e.key !== "Enter") {
        return;
      }

      this.focusedItem = null;
      const query = sanitizeQuery(this.state.inputValue);

      if (query) {
        this.props.searchSources(query);
      }
    };

    this.onEnterPress = () => {
      if (this.focusedItem && !this.state.inputFocused) {
        const {
          setExpanded,
          file,
          expanded,
          match
        } = this.focusedItem;

        if (setExpanded) {
          setExpanded(file, !expanded);
        } else if (match) {
          this.selectMatchItem(match);
        }
      }
    };

    this.inputOnChange = e => {
      const inputValue = e.target.value;
      const {
        clearSearch
      } = this.props;
      this.setState({
        inputValue
      });

      if (inputValue === "") {
        clearSearch();
      }
    };

    this.renderFile = (file, focused, expanded, setExpanded) => {
      if (focused) {
        this.focusedItem = {
          setExpanded,
          file,
          expanded
        };
      }

      const matchesLength = file.matches.length;
      const matches = ` (${matchesLength} match${matchesLength > 1 ? "es" : ""})`;
      return _react2.default.createElement("div", {
        className: (0, _classnames2.default)("file-result", {
          focused
        }),
        key: file.sourceId,
        onClick: e => setExpanded(file, !expanded)
      }, _react2.default.createElement(_Svg2.default, {
        name: "arrow",
        className: (0, _classnames2.default)({
          expanded
        })
      }), _react2.default.createElement("img", {
        className: "file"
      }), _react2.default.createElement("span", {
        className: "file-path"
      }, (0, _sourcesTree.getRelativePath)(file.filepath)), _react2.default.createElement("span", {
        className: "matches-summary"
      }, matches));
    };

    this.renderMatch = (match, focused) => {
      if (focused) {
        this.focusedItem = {
          match
        };
      }

      return _react2.default.createElement("div", {
        className: (0, _classnames2.default)("result", {
          focused
        }),
        onClick: () => setTimeout(() => this.selectMatchItem(match), 50)
      }, _react2.default.createElement("span", {
        className: "line-number",
        key: match.line
      }, match.line), (0, _projectSearch.highlightMatches)(match));
    };

    this.renderItem = (item, depth, focused, _, expanded, {
      setExpanded
    }) => {
      if (item.type === "RESULT") {
        return this.renderFile(item, focused, expanded, setExpanded);
      }

      return this.renderMatch(item, focused);
    };

    this.renderResults = () => {
      const results = this.getResults();
      const {
        status
      } = this.props;

      if (!this.props.query) {
        return;
      }

      if (results.length && status === _projectTextSearch.statusType.done) {
        return _react2.default.createElement(_ManagedTree2.default, {
          getRoots: () => results,
          getChildren: file => file.matches || [],
          itemHeight: 24,
          autoExpandAll: true,
          autoExpandDepth: 1,
          getParent: item => null,
          getPath: getFilePath,
          renderItem: this.renderItem
        });
      }

      const msg = status === _projectTextSearch.statusType.fetching ? L10N.getStr("loadingText") : L10N.getStr("projectTextSearch.noResults");
      return _react2.default.createElement("div", {
        className: "no-result-msg absolute-center"
      }, msg);
    };

    this.renderSummary = () => {
      return this.props.query !== "" ? L10N.getFormatStr("sourceSearch.resultsSummary1", this.getResultCount()) : "";
    };

    this.state = {
      inputValue: this.props.query || "",
      inputFocused: false
    };
  }

  componentDidMount() {
    const {
      shortcuts
    } = this.context;
    shortcuts.on(L10N.getStr("projectTextSearch.key"), this.toggleProjectTextSearch);
    shortcuts.on("Enter", this.onEnterPress);
  }

  componentWillUnmount() {
    const {
      shortcuts
    } = this.context;
    shortcuts.off(L10N.getStr("projectTextSearch.key"), this.toggleProjectTextSearch);
    shortcuts.off("Enter", this.onEnterPress);
  }

  shouldShowErrorEmoji() {
    return !this.getResultCount() && this.props.status === _projectTextSearch.statusType.done;
  }

  renderInput() {
    return _react2.default.createElement(_SearchInput2.default, {
      query: this.state.inputValue,
      count: this.getResultCount(),
      placeholder: L10N.getStr("projectTextSearch.placeholder"),
      size: "big",
      showErrorEmoji: this.shouldShowErrorEmoji(),
      summaryMsg: this.renderSummary(),
      onChange: this.inputOnChange,
      onFocus: () => this.setState({
        inputFocused: true
      }),
      onBlur: () => this.setState({
        inputFocused: false
      }),
      onKeyDown: this.onKeyDown,
      handleClose: this.props.closeProjectSearch,
      ref: "searchInput"
    });
  }

  render() {
    if (!this.isProjectSearchEnabled()) {
      return null;
    }

    return _react2.default.createElement("div", {
      className: "search-container"
    }, _react2.default.createElement("div", {
      className: "project-text-search"
    }, _react2.default.createElement("div", {
      className: "header"
    }, this.renderInput()), this.renderResults()));
  }

}

exports.ProjectSearch = ProjectSearch;
ProjectSearch.contextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => ({
  sources: (0, _selectors.getSources)(state),
  activeSearch: (0, _selectors.getActiveSearch)(state),
  results: (0, _selectors.getTextSearchResults)(state),
  query: (0, _selectors.getTextSearchQuery)(state),
  status: (0, _selectors.getTextSearchStatus)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  closeProjectSearch: _actions2.default.closeProjectSearch,
  searchSources: _actions2.default.searchSources,
  clearSearch: _actions2.default.clearSearch,
  selectLocation: _actions2.default.selectLocation,
  setActiveSearch: _actions2.default.setActiveSearch,
  doSearchForHighlight: _actions2.default.doSearchForHighlight
})(ProjectSearch);