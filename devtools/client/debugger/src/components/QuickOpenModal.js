/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../utils/connect";
import fuzzyAldrin from "fuzzaldrin-plus";
import { basename } from "../utils/path";

const { throttle } = require("devtools/shared/throttle");

import actions from "../actions";
import {
  getDisplayedSourcesList,
  getQuickOpenEnabled,
  getQuickOpenQuery,
  getQuickOpenType,
  getSelectedSource,
  getSourceContent,
  getSymbols,
  getTabs,
  getContext,
  getBlackBoxRanges,
  getProjectDirectoryRoot,
} from "../selectors";
import { memoizeLast } from "../utils/memoizeLast";
import { scrollList } from "../utils/result-list";
import {
  formatSymbols,
  parseLineColumn,
  formatShortcutResults,
  formatSourceForList,
} from "../utils/quick-open";
import Modal from "./shared/Modal";
import SearchInput from "./shared/SearchInput";
import ResultList from "./shared/ResultList";

import "./QuickOpenModal.css";

const maxResults = 100;

const SIZE_BIG = { size: "big" };
const SIZE_DEFAULT = {};

function filter(values, query) {
  const preparedQuery = fuzzyAldrin.prepareQuery(query);

  return fuzzyAldrin.filter(values, query, {
    key: "value",
    maxResults,
    preparedQuery,
  });
}

export class QuickOpenModal extends Component {
  // Put it on the class so it can be retrieved in tests
  static UPDATE_RESULTS_THROTTLE = 100;

  constructor(props) {
    super(props);
    this.state = { results: null, selectedIndex: 0 };
  }

  static get propTypes() {
    return {
      closeQuickOpen: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      displayedSources: PropTypes.array.isRequired,
      blackBoxRanges: PropTypes.object.isRequired,
      enabled: PropTypes.bool.isRequired,
      highlightLineRange: PropTypes.func.isRequired,
      query: PropTypes.string.isRequired,
      searchType: PropTypes.oneOf([
        "functions",
        "goto",
        "gotoSource",
        "other",
        "shortcuts",
        "sources",
        "variables",
      ]).isRequired,
      selectSpecificLocation: PropTypes.func.isRequired,
      selectedContentLoaded: PropTypes.bool,
      selectedSource: PropTypes.object,
      setQuickOpenQuery: PropTypes.func.isRequired,
      shortcutsModalEnabled: PropTypes.bool.isRequired,
      symbols: PropTypes.object.isRequired,
      symbolsLoading: PropTypes.bool.isRequired,
      tabUrls: PropTypes.array.isRequired,
      toggleShortcutsModal: PropTypes.func.isRequired,
    };
  }

  setResults(results) {
    if (results) {
      results = results.slice(0, maxResults);
    }
    this.setState({ results });
  }

  componentDidMount() {
    const { query, shortcutsModalEnabled, toggleShortcutsModal } = this.props;

    this.updateResults(query);

    if (shortcutsModalEnabled) {
      toggleShortcutsModal();
    }
  }

  componentDidUpdate(prevProps) {
    const nowEnabled = !prevProps.enabled && this.props.enabled;
    const queryChanged = prevProps.query !== this.props.query;

    if (this.refs.resultList && this.refs.resultList.refs) {
      scrollList(this.refs.resultList.refs, this.state.selectedIndex);
    }

    if (nowEnabled || queryChanged) {
      this.updateResults(this.props.query);
    }
  }

  closeModal = () => {
    this.props.closeQuickOpen();
  };

  dropGoto = query => {
    const index = query.indexOf(":");
    return index !== -1 ? query.slice(0, index) : query;
  };

  formatSources = memoizeLast(
    (displayedSources, tabUrls, blackBoxRanges, projectDirectoryRoot) => {
      // Note that we should format all displayed sources,
      // the actual filtering will only be done late from `searchSources()`
      return displayedSources.map(source => {
        const isBlackBoxed = !!blackBoxRanges[source.url];
        const hasTabOpened = tabUrls.includes(source.url);
        return formatSourceForList(
          source,
          hasTabOpened,
          isBlackBoxed,
          projectDirectoryRoot
        );
      });
    }
  );

  searchSources = query => {
    const {
      displayedSources,
      tabUrls,
      blackBoxRanges,
      projectDirectoryRoot,
    } = this.props;

    const sources = this.formatSources(
      displayedSources,
      tabUrls,
      blackBoxRanges,
      projectDirectoryRoot
    );
    const results =
      query == "" ? sources : filter(sources, this.dropGoto(query));
    return this.setResults(results);
  };

  searchSymbols = query => {
    const {
      symbols: { functions },
    } = this.props;

    let results = functions;
    results = results.filter(result => result.title !== "anonymous");

    if (query === "@" || query === "#") {
      return this.setResults(results);
    }
    results = filter(results, query.slice(1));
    return this.setResults(results);
  };

  searchShortcuts = query => {
    const results = formatShortcutResults();
    if (query == "?") {
      this.setResults(results);
    } else {
      this.setResults(filter(results, query.slice(1)));
    }
  };

  /**
   * This method is called when we just opened the modal and the query input is empty
   */
  showTopSources = () => {
    const { tabUrls, blackBoxRanges, projectDirectoryRoot } = this.props;
    let { displayedSources } = this.props;

    // If there is some tabs opened, only show tab's sources.
    // Otherwise, we display all visible sources (per SourceTree definition),
    // setResults will restrict the number of results to a maximum limit.
    if (tabUrls.length) {
      displayedSources = displayedSources.filter(
        source => !!source.url && tabUrls.includes(source.url)
      );
    }

    this.setResults(
      this.formatSources(
        displayedSources,
        tabUrls,
        blackBoxRanges,
        projectDirectoryRoot
      )
    );
  };

  updateResults = throttle(query => {
    if (this.isGotoQuery()) {
      return;
    }

    if (query == "" && !this.isShortcutQuery()) {
      this.showTopSources();
      return;
    }

    if (this.isSymbolSearch()) {
      this.searchSymbols(query);
      return;
    }

    if (this.isShortcutQuery()) {
      this.searchShortcuts(query);
      return;
    }

    this.searchSources(query);
  }, QuickOpenModal.UPDATE_RESULTS_THROTTLE);

  setModifier = item => {
    if (["@", "#", ":"].includes(item.id)) {
      this.props.setQuickOpenQuery(item.id);
    }
  };

  selectResultItem = (e, item) => {
    if (item == null) {
      return;
    }

    if (this.isShortcutQuery()) {
      this.setModifier(item);
      return;
    }

    if (this.isGotoSourceQuery()) {
      const location = parseLineColumn(this.props.query);
      this.gotoLocation({ ...location, sourceId: item.id });
      return;
    }

    if (this.isSymbolSearch()) {
      this.gotoLocation({
        line:
          item.location && item.location.start ? item.location.start.line : 0,
      });
      return;
    }

    this.gotoLocation({ sourceId: item.id, line: 0 });
  };

  onSelectResultItem = item => {
    const { selectedSource, highlightLineRange } = this.props;
    if (
      selectedSource == null ||
      !this.isSymbolSearch() ||
      !this.isFunctionQuery()
    ) {
      return;
    }

    highlightLineRange({
      ...(item.location != null
        ? { start: item.location.start.line, end: item.location.end.line }
        : {}),
      sourceId: selectedSource.id,
    });
  };

  traverseResults = e => {
    const direction = e.key === "ArrowUp" ? -1 : 1;
    const { selectedIndex, results } = this.state;
    const resultCount = this.getResultCount();
    const index = selectedIndex + direction;
    const nextIndex = (index + resultCount) % resultCount || 0;

    this.setState({ selectedIndex: nextIndex });

    if (results != null) {
      this.onSelectResultItem(results[nextIndex]);
    }
  };

  gotoLocation = location => {
    const { cx, selectSpecificLocation, selectedSource } = this.props;

    if (location != null) {
      const selectedSourceId = selectedSource ? selectedSource.id : "";
      const sourceId = location.sourceId ? location.sourceId : selectedSourceId;
      selectSpecificLocation(cx, {
        sourceId,
        line: location.line,
        column: location.column,
      });
      this.closeModal();
    }
  };

  onChange = e => {
    const {
      selectedSource,
      selectedContentLoaded,
      setQuickOpenQuery,
    } = this.props;
    setQuickOpenQuery(e.target.value);
    const noSource = !selectedSource || !selectedContentLoaded;
    if ((noSource && this.isSymbolSearch()) || this.isGotoQuery()) {
      return;
    }

    this.updateResults(e.target.value);
  };

  onKeyDown = e => {
    const { enabled, query } = this.props;
    const { results, selectedIndex } = this.state;
    const isGoToQuery = this.isGotoQuery();

    if ((!enabled || !results) && !isGoToQuery) {
      return;
    }

    if (e.key === "Enter") {
      if (isGoToQuery) {
        const location = parseLineColumn(query);
        this.gotoLocation(location);
        return;
      }

      if (results) {
        this.selectResultItem(e, results[selectedIndex]);
        return;
      }
    }

    if (e.key === "Tab") {
      this.closeModal();
      return;
    }

    if (["ArrowUp", "ArrowDown"].includes(e.key)) {
      e.preventDefault();
      this.traverseResults(e);
    }
  };

  getResultCount = () => {
    const { results } = this.state;
    return results && results.length ? results.length : 0;
  };

  // Query helpers
  isFunctionQuery = () => this.props.searchType === "functions";
  isSymbolSearch = () => this.isFunctionQuery();
  isGotoQuery = () => this.props.searchType === "goto";
  isGotoSourceQuery = () => this.props.searchType === "gotoSource";
  isShortcutQuery = () => this.props.searchType === "shortcuts";
  isSourcesQuery = () => this.props.searchType === "sources";
  isSourceSearch = () => this.isSourcesQuery() || this.isGotoSourceQuery();

  /* eslint-disable react/no-danger */
  renderHighlight(candidateString, query, name) {
    const options = {
      wrap: {
        tagOpen: '<mark class="highlight">',
        tagClose: "</mark>",
      },
    };
    const html = fuzzyAldrin.wrap(candidateString, query, options);
    return <div dangerouslySetInnerHTML={{ __html: html }} />;
  }

  highlightMatching = (query, results) => {
    let newQuery = query;
    if (newQuery === "") {
      return results;
    }
    newQuery = query.replace(/[@:#?]/gi, " ");

    return results.map(result => {
      if (typeof result.title == "string") {
        return {
          ...result,
          title: this.renderHighlight(
            result.title,
            basename(newQuery),
            "title"
          ),
        };
      }
      return result;
    });
  };

  shouldShowErrorEmoji() {
    const { query } = this.props;
    if (this.isGotoQuery()) {
      return !/^:\d*$/.test(query);
    }
    return !!query && !this.getResultCount();
  }

  getSummaryMessage() {
    let summaryMsg = "";
    if (this.isGotoQuery()) {
      summaryMsg = L10N.getStr("shortcuts.gotoLine");
    } else if (this.isFunctionQuery() && this.props.symbolsLoading) {
      summaryMsg = L10N.getStr("loadingText");
    }
    return summaryMsg;
  }

  render() {
    const { enabled, query } = this.props;
    const { selectedIndex, results } = this.state;

    if (!enabled) {
      return null;
    }
    const items = this.highlightMatching(query, results || []);
    const expanded = !!items && !!items.length;

    return (
      <Modal in={enabled} handleClose={this.closeModal}>
        <SearchInput
          query={query}
          hasPrefix={true}
          count={this.getResultCount()}
          placeholder={L10N.getStr("sourceSearch.search2")}
          summaryMsg={this.getSummaryMessage()}
          showErrorEmoji={this.shouldShowErrorEmoji()}
          isLoading={false}
          onChange={this.onChange}
          onKeyDown={this.onKeyDown}
          handleClose={this.closeModal}
          expanded={expanded}
          showClose={false}
          selectedItemId={
            expanded && items[selectedIndex] ? items[selectedIndex].id : ""
          }
          {...(this.isSourceSearch() ? SIZE_BIG : SIZE_DEFAULT)}
        />
        {results && (
          <ResultList
            key="results"
            items={items}
            selected={selectedIndex}
            selectItem={this.selectResultItem}
            ref="resultList"
            expanded={expanded}
            {...(this.isSourceSearch() ? SIZE_BIG : SIZE_DEFAULT)}
          />
        )}
      </Modal>
    );
  }
}

/* istanbul ignore next: ignoring testing of redux connection stuff */
function mapStateToProps(state) {
  const selectedSource = getSelectedSource(state);
  const displayedSources = getDisplayedSourcesList(state);
  const tabs = getTabs(state);
  const tabUrls = [...new Set(tabs.map(tab => tab.url))];
  const symbols = getSymbols(state, selectedSource);

  return {
    cx: getContext(state),
    enabled: getQuickOpenEnabled(state),
    displayedSources,
    blackBoxRanges: getBlackBoxRanges(state),
    projectDirectoryRoot: getProjectDirectoryRoot(state),
    selectedSource,
    selectedContentLoaded: selectedSource
      ? !!getSourceContent(state, selectedSource.id)
      : undefined,
    symbols: formatSymbols(symbols, maxResults),
    symbolsLoading: !symbols,
    query: getQuickOpenQuery(state),
    searchType: getQuickOpenType(state),
    tabUrls,
  };
}

/* istanbul ignore next: ignoring testing of redux connection stuff */
export default connect(mapStateToProps, {
  selectSpecificLocation: actions.selectSpecificLocation,
  setQuickOpenQuery: actions.setQuickOpenQuery,
  highlightLineRange: actions.highlightLineRange,
  closeQuickOpen: actions.closeQuickOpen,
})(QuickOpenModal);
