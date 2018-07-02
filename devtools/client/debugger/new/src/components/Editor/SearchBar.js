"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _editor = require("../../utils/editor/index");

var _resultList = require("../../utils/result-list");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _SearchInput = require("../shared/SearchInput");

var _SearchInput2 = _interopRequireDefault(_SearchInput);

var _lodash = require("devtools/client/shared/vendor/lodash");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getShortcuts() {
  const searchAgainKey = L10N.getStr("sourceSearch.search.again.key2");
  const searchAgainPrevKey = L10N.getStr("sourceSearch.search.againPrev.key2");
  const searchKey = L10N.getStr("sourceSearch.search.key2");
  return {
    shiftSearchAgainShortcut: searchAgainPrevKey,
    searchAgainShortcut: searchAgainKey,
    searchShortcut: searchKey
  };
}

class SearchBar extends _react.Component {
  constructor(props) {
    super(props);

    this.onEscape = e => {
      this.closeSearch(e);
    };

    this.clearSearch = () => {
      const {
        editor: ed,
        query
      } = this.props;

      if (ed) {
        const ctx = {
          ed,
          cm: ed.codeMirror
        };
        (0, _editor.removeOverlay)(ctx, query);
      }
    };

    this.closeSearch = e => {
      const {
        closeFileSearch,
        editor,
        searchOn
      } = this.props;

      if (editor && searchOn) {
        this.clearSearch();
        closeFileSearch(editor);
        e.stopPropagation();
        e.preventDefault();
      }

      this.setState({
        query: "",
        inputFocused: false
      });
    };

    this.toggleSearch = e => {
      e.stopPropagation();
      e.preventDefault();
      const {
        editor,
        searchOn,
        setActiveSearch
      } = this.props;

      if (!searchOn) {
        setActiveSearch("file");
      }

      if (searchOn && editor) {
        const query = editor.codeMirror.getSelection() || this.state.query;

        if (query !== "") {
          this.setState({
            query,
            inputFocused: true
          });
          this.doSearch(query);
        } else {
          this.setState({
            query: "",
            inputFocused: true
          });
        }
      }
    };

    this.doSearch = query => {
      const {
        selectedSource
      } = this.props;

      if (!selectedSource || !selectedSource.text) {
        return;
      }

      this.props.doSearch(query, this.props.editor);
    };

    this.updateSearchResults = (characterIndex, line, matches) => {
      const matchIndex = matches.findIndex(elm => elm.line === line && elm.ch === characterIndex);
      this.props.updateSearchResults({
        matches,
        matchIndex,
        count: matches.length,
        index: characterIndex
      });
    };

    this.traverseResults = (e, rev) => {
      e.stopPropagation();
      e.preventDefault();
      const editor = this.props.editor;

      if (!editor) {
        return;
      }

      this.props.traverseResults(rev, editor);
    };

    this.onChange = e => {
      this.setState({
        query: e.target.value
      });
      return this.doSearch(e.target.value);
    };

    this.onBlur = e => {
      this.setState({
        inputFocused: false
      });
    };

    this.onKeyDown = e => {
      if (e.key !== "Enter" && e.key !== "F3") {
        return;
      }

      this.traverseResults(e, e.shiftKey);
      e.preventDefault();
      return this.doSearch(e.target.value);
    };

    this.renderSearchModifiers = () => {
      const {
        modifiers,
        toggleFileSearchModifier,
        query
      } = this.props;
      const {
        doSearch
      } = this;

      function SearchModBtn({
        modVal,
        className,
        svgName,
        tooltip
      }) {
        const preppedClass = (0, _classnames2.default)(className, {
          active: modifiers && modifiers.get(modVal)
        });
        return _react2.default.createElement("button", {
          className: preppedClass,
          onClick: () => {
            toggleFileSearchModifier(modVal);
            doSearch(query);
          },
          title: tooltip
        }, _react2.default.createElement(_Svg2.default, {
          name: svgName
        }));
      }

      return _react2.default.createElement("div", {
        className: "search-modifiers"
      }, _react2.default.createElement("span", {
        className: "search-type-name"
      }, L10N.getStr("symbolSearch.searchModifier.modifiersLabel")), _react2.default.createElement(SearchModBtn, {
        modVal: "regexMatch",
        className: "regex-match-btn",
        svgName: "regex-match",
        tooltip: L10N.getStr("symbolSearch.searchModifier.regex")
      }), _react2.default.createElement(SearchModBtn, {
        modVal: "caseSensitive",
        className: "case-sensitive-btn",
        svgName: "case-match",
        tooltip: L10N.getStr("symbolSearch.searchModifier.caseSensitive")
      }), _react2.default.createElement(SearchModBtn, {
        modVal: "wholeWord",
        className: "whole-word-btn",
        svgName: "whole-word-match",
        tooltip: L10N.getStr("symbolSearch.searchModifier.wholeWord")
      }));
    };

    this.state = {
      query: props.query,
      selectedResultIndex: 0,
      count: 0,
      index: -1,
      inputFocused: false
    };
  }

  componentWillUnmount() {
    const shortcuts = this.context.shortcuts;
    const {
      searchShortcut,
      searchAgainShortcut,
      shiftSearchAgainShortcut
    } = getShortcuts();
    shortcuts.off(searchShortcut);
    shortcuts.off("Escape");
    shortcuts.off(searchAgainShortcut);
    shortcuts.off(shiftSearchAgainShortcut);
  }

  componentDidMount() {
    // overwrite this.doSearch with debounced version to
    // reduce frequency of queries
    this.doSearch = (0, _lodash.debounce)(this.doSearch, 100);
    const shortcuts = this.context.shortcuts;
    const {
      searchShortcut,
      searchAgainShortcut,
      shiftSearchAgainShortcut
    } = getShortcuts();
    shortcuts.on(searchShortcut, (_, e) => this.toggleSearch(e));
    shortcuts.on("Escape", (_, e) => this.onEscape(e));
    shortcuts.on(shiftSearchAgainShortcut, (_, e) => this.traverseResults(e, true));
    shortcuts.on(searchAgainShortcut, (_, e) => this.traverseResults(e, false));
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.refs.resultList && this.refs.resultList.refs) {
      (0, _resultList.scrollList)(this.refs.resultList.refs, this.state.selectedResultIndex);
    }
  }

  // Renderers
  buildSummaryMsg() {
    const {
      searchResults: {
        matchIndex,
        count,
        index
      },
      query
    } = this.props;

    if (query.trim() == "") {
      return "";
    }

    if (count == 0) {
      return L10N.getStr("editor.noResults");
    }

    if (index == -1) {
      return L10N.getFormatStr("sourceSearch.resultsSummary1", count);
    }

    return L10N.getFormatStr("editor.searchResults", matchIndex + 1, count);
  }

  shouldShowErrorEmoji() {
    const {
      query,
      searchResults: {
        count
      }
    } = this.props;
    return !!query && !count;
  }

  render() {
    const {
      searchResults: {
        count
      },
      searchOn
    } = this.props;

    if (!searchOn) {
      return _react2.default.createElement("div", null);
    }

    return _react2.default.createElement("div", {
      className: "search-bar"
    }, _react2.default.createElement(_SearchInput2.default, {
      query: this.state.query,
      count: count,
      placeholder: L10N.getStr("sourceSearch.search.placeholder"),
      summaryMsg: this.buildSummaryMsg(),
      onChange: this.onChange,
      onBlur: this.onBlur,
      showErrorEmoji: this.shouldShowErrorEmoji(),
      onKeyDown: this.onKeyDown,
      handleNext: e => this.traverseResults(e, false),
      handlePrev: e => this.traverseResults(e, true),
      handleClose: this.closeSearch,
      shouldFocus: this.state.inputFocused
    }), _react2.default.createElement("div", {
      className: "search-bottom-bar"
    }, this.renderSearchModifiers()));
  }

}

SearchBar.contextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => ({
  searchOn: (0, _selectors.getActiveSearch)(state) === "file",
  selectedSource: (0, _selectors.getSelectedSource)(state),
  selectedLocation: (0, _selectors.getSelectedLocation)(state),
  query: (0, _selectors.getFileSearchQuery)(state),
  modifiers: (0, _selectors.getFileSearchModifiers)(state),
  highlightedLineRange: (0, _selectors.getHighlightedLineRange)(state),
  searchResults: (0, _selectors.getFileSearchResults)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  toggleFileSearchModifier: _actions2.default.toggleFileSearchModifier,
  setFileSearchQuery: _actions2.default.setFileSearchQuery,
  setActiveSearch: _actions2.default.setActiveSearch,
  closeFileSearch: _actions2.default.closeFileSearch,
  doSearch: _actions2.default.doSearch,
  traverseResults: _actions2.default.traverseResults,
  updateSearchResults: _actions2.default.updateSearchResults
})(SearchBar);