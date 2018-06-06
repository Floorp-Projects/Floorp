"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.QuickOpenModal = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _fuzzaldrinPlus = require("devtools/client/debugger/new/dist/vendors").vendored["fuzzaldrin-plus"];

var _fuzzaldrinPlus2 = _interopRequireDefault(_fuzzaldrinPlus);

var _path = require("../utils/path");

var _actions = require("../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../selectors/index");

var _resultList = require("../utils/result-list");

var _quickOpen = require("../utils/quick-open");

var _Modal = require("./shared/Modal");

var _Modal2 = _interopRequireDefault(_Modal);

var _SearchInput = require("./shared/SearchInput");

var _SearchInput2 = _interopRequireDefault(_SearchInput);

var _ResultList = require("./shared/ResultList");

var _ResultList2 = _interopRequireDefault(_ResultList);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function filter(values, query) {
  return _fuzzaldrinPlus2.default.filter(values, query, {
    key: "value",
    maxResults: 1000
  });
}

class QuickOpenModal extends _react.Component {
  constructor(props) {
    super(props);

    this.closeModal = () => {
      this.props.closeQuickOpen();
    };

    this.dropGoto = query => {
      return query.split(":")[0];
    };

    this.searchSources = query => {
      const {
        sources
      } = this.props;
      const results = query == "" ? sources : filter(sources, this.dropGoto(query));
      return this.setState({
        results
      });
    };

    this.searchSymbols = query => {
      const {
        symbols: {
          functions,
          variables
        }
      } = this.props;
      let results = functions;

      if (this.isVariableQuery()) {
        results = variables;
      } else {
        results = results.filter(result => result.title !== "anonymous");
      }

      if (query === "@" || query === "#") {
        return this.setState({
          results
        });
      }

      this.setState({
        results: filter(results, query.slice(1))
      });
    };

    this.searchShortcuts = query => {
      const results = (0, _quickOpen.formatShortcutResults)();

      if (query == "?") {
        this.setState({
          results
        });
      } else {
        this.setState({
          results: filter(results, query.slice(1))
        });
      }
    };

    this.showTopSources = () => {
      const {
        tabs,
        sources
      } = this.props;

      if (tabs.length > 0) {
        this.setState({
          results: sources.filter(source => tabs.includes(source.url))
        });
      } else {
        this.setState({
          results: sources.slice(0, 100)
        });
      }
    };

    this.updateResults = query => {
      if (this.isGotoQuery()) {
        return;
      }

      if (query == "" && !this.isShortcutQuery()) {
        return this.showTopSources();
      }

      if (this.isSymbolSearch()) {
        return this.searchSymbols(query);
      }

      if (this.isShortcutQuery()) {
        return this.searchShortcuts(query);
      }

      return this.searchSources(query);
    };

    this.setModifier = item => {
      if (["@", "#", ":"].includes(item.id)) {
        this.props.setQuickOpenQuery(item.id);
      }
    };

    this.selectResultItem = (e, item) => {
      if (item == null) {
        return;
      }

      if (this.isShortcutQuery()) {
        return this.setModifier(item);
      }

      if (this.isGotoSourceQuery()) {
        const location = (0, _quickOpen.parseLineColumn)(this.props.query);
        return this.gotoLocation(_objectSpread({}, location, {
          sourceId: item.id
        }));
      }

      if (this.isSymbolSearch()) {
        return this.gotoLocation({
          line: item.location && item.location.start ? item.location.start.line : 0
        });
      }

      this.gotoLocation({
        sourceId: item.id,
        line: 0
      });
    };

    this.onSelectResultItem = item => {
      const {
        selectLocation,
        selectedSource,
        highlightLineRange
      } = this.props;

      if (!this.isSymbolSearch() || selectedSource == null) {
        return;
      }

      if (this.isVariableQuery()) {
        const line = item.location && item.location.start ? item.location.start.line : 0;
        return selectLocation({
          sourceId: selectedSource.get("id"),
          line,
          column: null
        });
      }

      if (this.isFunctionQuery()) {
        return highlightLineRange(_objectSpread({}, item.location != null ? {
          start: item.location.start.line,
          end: item.location.end.line
        } : {}, {
          sourceId: selectedSource.get("id")
        }));
      }
    };

    this.traverseResults = e => {
      const direction = e.key === "ArrowUp" ? -1 : 1;
      const {
        selectedIndex,
        results
      } = this.state;
      const resultCount = this.getResultCount();
      const index = selectedIndex + direction;
      const nextIndex = (index + resultCount) % resultCount;
      this.setState({
        selectedIndex: nextIndex
      });

      if (results != null) {
        this.onSelectResultItem(results[nextIndex]);
      }
    };

    this.gotoLocation = location => {
      const {
        selectLocation,
        selectedSource
      } = this.props;
      const selectedSourceId = selectedSource ? selectedSource.get("id") : "";

      if (location != null) {
        const sourceId = location.sourceId ? location.sourceId : selectedSourceId;
        selectLocation({
          sourceId,
          line: location.line,
          column: location.column || null
        });
        this.closeModal();
      }
    };

    this.onChange = e => {
      const {
        selectedSource,
        setQuickOpenQuery
      } = this.props;
      setQuickOpenQuery(e.target.value);
      const noSource = !selectedSource || !selectedSource.get("text");

      if (this.isSymbolSearch() && noSource || this.isGotoQuery()) {
        return;
      }

      this.updateResults(e.target.value);
    };

    this.onKeyDown = e => {
      const {
        enabled,
        query
      } = this.props;
      const {
        results,
        selectedIndex
      } = this.state;

      if (!this.isGotoQuery() && (!enabled || !results)) {
        return;
      }

      if (e.key === "Enter") {
        if (this.isGotoQuery()) {
          const location = (0, _quickOpen.parseLineColumn)(query);
          return this.gotoLocation(location);
        }

        if (results) {
          return this.selectResultItem(e, results[selectedIndex]);
        }
      }

      if (e.key === "Tab") {
        return this.closeModal();
      }

      if (["ArrowUp", "ArrowDown"].includes(e.key)) {
        e.preventDefault();
        return this.traverseResults(e);
      }
    };

    this.getResultCount = () => {
      const results = this.state.results;
      return results && results.length ? results.length : 0;
    };

    this.isFunctionQuery = () => this.props.searchType === "functions";

    this.isVariableQuery = () => this.props.searchType === "variables";

    this.isSymbolSearch = () => this.isFunctionQuery() || this.isVariableQuery();

    this.isGotoQuery = () => this.props.searchType === "goto";

    this.isGotoSourceQuery = () => this.props.searchType === "gotoSource";

    this.isShortcutQuery = () => this.props.searchType === "shortcuts";

    this.isSourcesQuery = () => this.props.searchType === "sources";

    this.isSourceSearch = () => this.isSourcesQuery() || this.isGotoSourceQuery();

    this.renderHighlight = (candidateString, query, name) => {
      const options = {
        wrap: {
          tagOpen: '<mark class="highlight">',
          tagClose: "</mark>"
        }
      };

      const html = _fuzzaldrinPlus2.default.wrap(candidateString, query, options);

      return _react2.default.createElement("div", {
        dangerouslySetInnerHTML: {
          __html: html
        }
      });
    };

    this.highlightMatching = (query, results) => {
      let newQuery = query;

      if (newQuery === "") {
        return results;
      }

      newQuery = query.replace(/[@:#?]/gi, " ");
      return results.map(result => {
        return _objectSpread({}, result, {
          title: this.renderHighlight(result.title, (0, _path.basename)(newQuery), "title")
        });
      });
    };

    this.state = {
      results: null,
      selectedIndex: 0
    };
  }

  componentDidMount() {
    const {
      query,
      shortcutsModalEnabled,
      toggleShortcutsModal
    } = this.props;
    this.updateResults(query);

    if (shortcutsModalEnabled) {
      toggleShortcutsModal();
    }
  }

  componentDidUpdate(prevProps) {
    const nowEnabled = !prevProps.enabled && this.props.enabled;
    const queryChanged = prevProps.query !== this.props.query;

    if (this.refs.resultList && this.refs.resultList.refs) {
      (0, _resultList.scrollList)(this.refs.resultList.refs, this.state.selectedIndex, nowEnabled || !queryChanged);
    }

    if (nowEnabled || queryChanged) {
      this.updateResults(this.props.query);
    }
  }

  shouldShowErrorEmoji() {
    const {
      query
    } = this.props;

    if (this.isGotoQuery()) {
      return !/^:\d*$/.test(query);
    }

    return !this.getResultCount() && !!query;
  }

  getSummaryMessage() {
    let summaryMsg = "";

    if (this.isGotoQuery()) {
      summaryMsg = L10N.getStr("shortcuts.gotoLine");
    } else if ((this.isFunctionQuery() || this.isVariableQuery()) && this.props.symbolsLoading) {
      summaryMsg = L10N.getStr("loadingText");
    }

    return summaryMsg;
  }

  render() {
    const {
      enabled,
      query
    } = this.props;
    const {
      selectedIndex,
      results
    } = this.state;

    if (!enabled) {
      return null;
    }

    const newResults = results && results.slice(0, 100);
    const items = this.highlightMatching(query, newResults || []);
    const expanded = !!items && items.length > 0;
    return _react2.default.createElement(_Modal2.default, {
      "in": enabled,
      handleClose: this.closeModal
    }, _react2.default.createElement(_SearchInput2.default, _extends({
      query: query,
      hasPrefix: true,
      count: this.getResultCount(),
      placeholder: L10N.getStr("sourceSearch.search"),
      summaryMsg: this.getSummaryMessage(),
      showErrorEmoji: this.shouldShowErrorEmoji(),
      onChange: this.onChange,
      onKeyDown: this.onKeyDown,
      handleClose: this.closeModal,
      expanded: expanded,
      showClose: false,
      selectedItemId: expanded && items[selectedIndex] ? items[selectedIndex].id : ""
    }, this.isSourceSearch() ? {
      size: "big"
    } : {})), newResults && _react2.default.createElement(_ResultList2.default, _extends({
      key: "results",
      items: items,
      selected: selectedIndex,
      selectItem: this.selectResultItem,
      ref: "resultList",
      expanded: expanded
    }, this.isSourceSearch() ? {
      size: "big"
    } : {})));
  }

}

exports.QuickOpenModal = QuickOpenModal;
/* istanbul ignore next: ignoring testing of redux connection stuff */

function mapStateToProps(state) {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  return {
    enabled: (0, _selectors.getQuickOpenEnabled)(state),
    sources: (0, _quickOpen.formatSources)((0, _selectors.getRelativeSources)(state), (0, _selectors.getTabs)(state).toArray()),
    selectedSource,
    symbols: (0, _quickOpen.formatSymbols)((0, _selectors.getSymbols)(state, selectedSource)),
    symbolsLoading: (0, _selectors.isSymbolsLoading)(state, selectedSource),
    query: (0, _selectors.getQuickOpenQuery)(state),
    searchType: (0, _selectors.getQuickOpenType)(state),
    tabs: (0, _selectors.getTabs)(state).toArray()
  };
}
/* istanbul ignore next: ignoring testing of redux connection stuff */


exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(QuickOpenModal);