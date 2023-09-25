/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import actions from "../../actions";

import { getEditor } from "../../utils/editor";
import { searchKeys } from "../../constants";

import { getRelativePath } from "../../utils/sources-tree/utils";
import { getFormattedSourceId } from "../../utils/source";
import { getProjectSearchQuery } from "../../selectors";

import SearchInput from "../shared/SearchInput";
import AccessibleImage from "../shared/AccessibleImage";

const { PluralForm } = require("devtools/shared/plural-form");
const classnames = require("devtools/client/shared/classnames.js");
const Tree = require("devtools/client/shared/components/Tree");
const { debounce } = require("devtools/shared/debounce");
const { throttle } = require("devtools/shared/throttle");

import "./ProjectSearch.css";

export const statusType = {
  initial: "INITIAL",
  fetching: "FETCHING",
  cancelled: "CANCELLED",
  done: "DONE",
  error: "ERROR",
};

function getFilePath(item, index) {
  return item.type === "RESULT"
    ? `${item.location.source.id}-${index || "$"}`
    : `${item.location.source.id}-${item.location.line}-${
        item.location.column
      }-${index || "$"}`;
}

export class ProjectSearch extends Component {
  constructor(props) {
    super(props);

    this.state = {
      // We may restore a previous state when changing tabs in the primary panes,
      // or when restoring primary panes from collapse.
      query: this.props.query || "",

      inputFocused: false,
      focusedItem: null,
      expanded: new Set(),
      results: [],
      status: statusType.done,
    };
    // Use throttle for updating results in order to prevent delaying showing result until the end of the search
    this.onUpdatedResults = throttle(this.onUpdatedResults.bind(this), 100);
    // Use debounce for input processing in order to wait for the end of user input edition before triggerring the search
    this.doSearch = debounce(this.doSearch.bind(this), 100);
    this.doSearch();
  }

  static get propTypes() {
    return {
      doSearchForHighlight: PropTypes.func.isRequired,
      query: PropTypes.string.isRequired,
      results: PropTypes.array.isRequired,
      searchSources: PropTypes.func.isRequired,
      selectSpecificLocation: PropTypes.func.isRequired,
      status: PropTypes.oneOf([
        "INITIAL",
        "FETCHING",
        "CANCELED",
        "DONE",
        "ERROR",
      ]).isRequired,
      modifiers: PropTypes.object,
      toggleProjectSearchModifier: PropTypes.func,
    };
  }

  componentDidMount() {
    const { shortcuts } = this.context;
    shortcuts.on("Enter", this.onEnterPress);
  }

  componentWillUnmount() {
    const { shortcuts } = this.context;
    shortcuts.off("Enter", this.onEnterPress);
  }

  async doSearch() {
    // Cancel any previous async ongoing search
    if (this.searchAbortController) {
      this.searchAbortController.abort();
    }

    if (!this.state.query) {
      this.setState({ status: statusType.done });
      return;
    }

    this.setState({ status: statusType.fetching, results: [] });

    // Setup an AbortController whose main goal is to be able to cancel the asynchronous
    // operation done by the `searchSources` action.
    // This allows allows the React Component to receive partial updates
    // to render results as they are available.
    this.searchAbortController = new AbortController();

    await this.props.searchSources(
      this.state.query,
      this.onUpdatedResults,
      this.searchAbortController.signal
    );
  }

  onUpdatedResults(results, done, signal) {
    // debounce may delay the execution after this search has been cancelled
    if (signal.aborted) {
      return;
    }

    this.setState({
      results,
      status: done ? statusType.done : statusType.fetching,
    });
  }

  selectMatchItem = matchItem => {
    this.props.selectSpecificLocation(matchItem.location);
    this.props.doSearchForHighlight(
      this.state.query,
      getEditor(),
      matchItem.location.line,
      matchItem.location.column
    );
  };

  highlightMatches = lineMatch => {
    const { value, matchIndex, match } = lineMatch;
    const len = match.length;
    return span(
      {
        className: "line-value",
      },
      span(
        {
          className: "line-match",
          key: 0,
        },
        value.slice(0, matchIndex)
      ),
      span(
        {
          className: "query-match",
          key: 1,
        },
        value.substr(matchIndex, len)
      ),
      span(
        {
          className: "line-match",
          key: 2,
        },
        value.slice(matchIndex + len, value.length)
      )
    );
  };

  getResultCount = () =>
    this.state.results.reduce((count, file) => count + file.matches.length, 0);

  onKeyDown = e => {
    if (e.key === "Escape") {
      return;
    }

    e.stopPropagation();

    this.setState({ focusedItem: null });
    this.doSearch();
  };

  onHistoryScroll = query => {
    this.setState({ query });
    this.doSearch();
  };

  onEnterPress = () => {
    // This is to select a match from the search result.
    if (!this.state.focusedItem || this.state.inputFocused) {
      return;
    }
    if (this.state.focusedItem.type === "MATCH") {
      this.selectMatchItem(this.state.focusedItem);
    }
  };

  onFocus = item => {
    if (this.state.focusedItem !== item) {
      this.setState({
        focusedItem: item,
      });
    }
  };

  inputOnChange = e => {
    const inputValue = e.target.value;
    this.setState({ query: inputValue });
    this.doSearch();
  };

  renderFile = (file, focused, expanded) => {
    const matchesLength = file.matches.length;
    const matches = ` (${matchesLength} match${matchesLength > 1 ? "es" : ""})`;
    return div(
      {
        className: classnames("file-result", {
          focused,
        }),
        key: file.location.source.id,
      },
      React.createElement(AccessibleImage, {
        className: classnames("arrow", {
          expanded,
        }),
      }),
      React.createElement(AccessibleImage, {
        className: "file",
      }),
      span(
        {
          className: "file-path",
        },
        file.location.source.url
          ? getRelativePath(file.location.source.url)
          : getFormattedSourceId(file.location.source.id)
      ),
      span(
        {
          className: "matches-summary",
        },
        matches
      )
    );
  };

  renderMatch = (match, focused) => {
    return div(
      {
        className: classnames("result", {
          focused,
        }),
        onClick: () => setTimeout(() => this.selectMatchItem(match), 50),
      },
      span(
        {
          className: "line-number",
          key: match.location.line,
        },
        match.location.line
      ),
      this.highlightMatches(match)
    );
  };

  renderItem = (item, depth, focused, _, expanded) => {
    if (item.type === "RESULT") {
      return this.renderFile(item, focused, expanded);
    }
    return this.renderMatch(item, focused);
  };

  renderResults = () => {
    const { status, results } = this.state;
    if (!this.state.query) {
      return null;
    }
    if (results.length) {
      return React.createElement(Tree, {
        getRoots: () => results,
        getChildren: file => file.matches || [],
        itemHeight: 24,
        autoExpandAll: true,
        autoExpandDepth: 1,
        autoExpandNodeChildrenLimit: 100,
        getParent: item => null,
        getPath: getFilePath,
        renderItem: this.renderItem,
        focused: this.state.focusedItem,
        onFocus: this.onFocus,
        isExpanded: item => {
          return this.state.expanded.has(item);
        },
        onExpand: item => {
          const { expanded } = this.state;
          expanded.add(item);
          this.setState({
            expanded,
          });
        },
        onCollapse: item => {
          const { expanded } = this.state;
          expanded.delete(item);
          this.setState({
            expanded,
          });
        },
        getKey: getFilePath,
      });
    }
    const msg =
      status === statusType.fetching
        ? L10N.getStr("loadingText")
        : L10N.getStr("projectTextSearch.noResults");
    return div(
      {
        className: "no-result-msg absolute-center",
      },
      msg
    );
  };

  renderSummary = () => {
    if (this.state.query !== "") {
      const resultsSummaryString = L10N.getStr("sourceSearch.resultsSummary2");
      const count = this.getResultCount();
      return PluralForm.get(count, resultsSummaryString).replace("#1", count);
    }
    return "";
  };

  shouldShowErrorEmoji() {
    return !this.getResultCount() && this.state.status === statusType.done;
  }

  renderInput() {
    const { status } = this.state;
    return React.createElement(SearchInput, {
      query: this.state.query,
      count: this.getResultCount(),
      placeholder: L10N.getStr("projectTextSearch.placeholder"),
      size: "small",
      showErrorEmoji: this.shouldShowErrorEmoji(),
      summaryMsg: this.renderSummary(),
      isLoading: status === statusType.fetching,
      onChange: this.inputOnChange,
      onFocus: () =>
        this.setState({
          inputFocused: true,
        }),
      onBlur: () =>
        this.setState({
          inputFocused: false,
        }),
      onKeyDown: this.onKeyDown,
      onHistoryScroll: this.onHistoryScroll,
      showClose: false,
      showExcludePatterns: true,
      excludePatternsLabel: L10N.getStr(
        "projectTextSearch.excludePatterns.label"
      ),
      excludePatternsPlaceholder: L10N.getStr(
        "projectTextSearch.excludePatterns.placeholder"
      ),
      ref: "searchInput",
      showSearchModifiers: true,
      searchKey: searchKeys.PROJECT_SEARCH,
      onToggleSearchModifier: this.doSearch,
    });
  }

  render() {
    return div(
      {
        className: "search-container",
      },
      div(
        {
          className: "project-text-search",
        },
        div(
          {
            className: "header",
          },
          this.renderInput()
        ),
        this.renderResults()
      )
    );
  }
}

ProjectSearch.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => ({
  query: getProjectSearchQuery(state),
});

export default connect(mapStateToProps, {
  searchSources: actions.searchSources,
  selectSpecificLocation: actions.selectSpecificLocation,
  doSearchForHighlight: actions.doSearchForHighlight,
})(ProjectSearch);
