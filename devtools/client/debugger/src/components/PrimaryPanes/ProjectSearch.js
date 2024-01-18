/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import {
  button,
  div,
  span,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../actions/index";

import { getEditor } from "../../utils/editor/index";
import { searchKeys } from "../../constants";

import { getRelativePath } from "../../utils/sources-tree/utils";
import { getFormattedSourceId } from "../../utils/source";
import {
  getProjectSearchQuery,
  getNavigateCounter,
} from "../../selectors/index";

import SearchInput from "../shared/SearchInput";
import AccessibleImage from "../shared/AccessibleImage";

const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const classnames = require("resource://devtools/client/shared/classnames.js");
const Tree = require("resource://devtools/client/shared/components/Tree.js");
const { debounce } = require("resource://devtools/shared/debounce.js");
const { throttle } = require("resource://devtools/shared/throttle.js");

const {
  HTMLTooltip,
} = require("resource://devtools/client/shared/widgets/tooltip/HTMLTooltip.js");

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
      navigateCounter: null,
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
      selectSpecificLocationOrSameUrl: PropTypes.func.isRequired,
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

  async doSearch() {
    // Cancel any previous async ongoing search
    if (this.searchAbortController) {
      this.searchAbortController.abort();
    }

    if (!this.state.query) {
      this.setState({ status: statusType.done });
      return;
    }

    this.setState({
      status: statusType.fetching,
      results: [],
      navigateCounter: this.props.navigateCounter,
    });

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

  selectMatchItem = async matchItem => {
    const foundMatchingSource =
      await this.props.selectSpecificLocationOrSameUrl(matchItem.location);
    // When we reload, or if the source's target has been destroyed,
    // we may no longer have the source available in the reducer.
    // In such case `selectSpecificLocationOrSameUrl` will return false.
    if (!foundMatchingSource) {
      // When going over results via the key arrows and Enter, we may display many tooltips at once.
      if (this.tooltip) {
        this.tooltip.hide();
      }
      // Go down to line-number otherwise HTMLTooltip's call to getBoundingClientRect would return (0, 0) position for the tooltip
      const element = document.querySelector(
        ".project-text-search .tree-node.focused .result .line-number"
      );
      const tooltip = new HTMLTooltip(element.ownerDocument, {
        className: "unavailable-source",
        type: "arrow",
      });
      tooltip.panel.textContent = L10N.getStr(
        "projectTextSearch.sourceNoLongerAvailable"
      );
      tooltip.setContentSize({ height: "auto" });
      tooltip.show(element);
      this.tooltip = tooltip;
      return;
    }
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

  // This can be called by Tree when manually selecting node via arrow keys and Enter.
  onActivate = item => {
    if (item && item.type === "MATCH") {
      this.selectMatchItem(item);
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
        onClick: () => this.selectMatchItem(match),
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

  renderRefreshButton() {
    if (!this.state.query) {
      return null;
    }

    // Highlight the refresh button when the current search results
    // are based on the previous document. doSearch will save the "navigate counter"
    // into state, while props will report the current "navigate counter".
    // The "navigate counter" is incremented each time we navigate to a new page.
    const highlight =
      this.state.navigateCounter != null &&
      this.state.navigateCounter != this.props.navigateCounter;
    return button(
      {
        className: classnames("refresh-btn devtools-button", {
          highlight,
        }),
        title: highlight
          ? L10N.getStr("projectTextSearch.refreshButtonTooltipOnNavigation")
          : L10N.getStr("projectTextSearch.refreshButtonTooltip"),
        onClick: this.doSearch,
      },
      React.createElement(AccessibleImage, {
        className: "refresh",
      })
    );
  }

  renderResultsToolbar() {
    if (!this.state.query) {
      return null;
    }
    return div(
      { className: "project-search-results-toolbar" },
      span({ className: "results-count" }, this.renderSummary()),
      this.renderRefreshButton()
    );
  }

  renderResults() {
    const { status, results } = this.state;
    if (!this.state.query) {
      return null;
    }
    if (results.length) {
      return React.createElement(Tree, {
        getRoots: () => results,
        getChildren: file => file.matches || [],
        autoExpandAll: true,
        autoExpandDepth: 1,
        autoExpandNodeChildrenLimit: 100,
        getParent: item => null,
        getPath: getFilePath,
        renderItem: this.renderItem,
        focused: this.state.focusedItem,
        onFocus: this.onFocus,
        onActivate: this.onActivate,
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
  }

  renderSummary = () => {
    if (this.state.query === "") {
      return "";
    }
    const resultsSummaryString = L10N.getStr("sourceSearch.resultsSummary2");
    const count = this.getResultCount();
    if (count === 0) {
      return "";
    }
    return PluralForm.get(count, resultsSummaryString).replace("#1", count);
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
        this.renderResultsToolbar(),
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
  navigateCounter: getNavigateCounter(state),
});

export default connect(mapStateToProps, {
  searchSources: actions.searchSources,
  selectSpecificLocationOrSameUrl: actions.selectSpecificLocationOrSameUrl,
  doSearchForHighlight: actions.doSearchForHighlight,
})(ProjectSearch);
