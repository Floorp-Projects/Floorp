/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import actions from "../../actions";

import { getEditor } from "../../utils/editor";
import { searchKeys } from "../../constants";

import { statusType } from "../../reducers/project-text-search";
import { getRelativePath } from "../../utils/sources-tree/utils";
import { getFormattedSourceId } from "../../utils/source";
import {
  getProjectSearchResults,
  getProjectSearchStatus,
  getProjectSearchQuery,
  getContext,
} from "../../selectors";

import SearchInput from "../shared/SearchInput";
import AccessibleImage from "../shared/AccessibleImage";

const { PluralForm } = require("devtools/shared/plural-form");
const classnames = require("devtools/client/shared/classnames.js");
const Tree = require("devtools/client/shared/components/Tree");

import "./ProjectSearch.css";

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
      inputValue: this.props.query || "",
      inputFocused: false,
      focusedItem: null,
      expanded: new Set(),
    };
  }

  static get propTypes() {
    return {
      clearSearch: PropTypes.func.isRequired,
      doSearchForHighlight: PropTypes.func.isRequired,
      query: PropTypes.string.isRequired,
      results: PropTypes.array.isRequired,
      searchSources: PropTypes.func.isRequired,
      selectSpecificLocation: PropTypes.func.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
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

  componentDidUpdate(prevProps) {
    // If the query changes in redux, also change it in the UI
    if (prevProps.query !== this.props.query) {
      this.setState({ inputValue: this.props.query });
    }
  }

  doSearch(searchTerm) {
    if (searchTerm) {
      this.props.searchSources(this.props.cx, searchTerm);
    }
  }

  selectMatchItem = matchItem => {
    this.props.selectSpecificLocation(matchItem.location);
    this.props.doSearchForHighlight(
      this.state.inputValue,
      getEditor(),
      matchItem.location.line,
      matchItem.location.column
    );
  };

  highlightMatches = lineMatch => {
    const { value, matchIndex, match } = lineMatch;
    const len = match.length;

    return (
      <span className="line-value">
        <span className="line-match" key={0}>
          {value.slice(0, matchIndex)}
        </span>
        <span className="query-match" key={1}>
          {value.substr(matchIndex, len)}
        </span>
        <span className="line-match" key={2}>
          {value.slice(matchIndex + len, value.length)}
        </span>
      </span>
    );
  };

  getResultCount = () =>
    this.props.results.reduce((count, file) => count + file.matches.length, 0);

  onKeyDown = e => {
    if (e.key === "Escape") {
      return;
    }

    e.stopPropagation();

    this.setState({ focusedItem: null });
    this.doSearch(this.state.inputValue);
  };

  onHistoryScroll = query => {
    this.setState({ inputValue: query });
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
      this.setState({ focusedItem: item });
    }
  };

  inputOnChange = e => {
    const inputValue = e.target.value;
    const { cx, clearSearch } = this.props;
    this.setState({ inputValue });
    if (inputValue === "") {
      clearSearch(cx);
    }
  };

  renderFile = (file, focused, expanded) => {
    const matchesLength = file.matches.length;
    const matches = ` (${matchesLength} match${matchesLength > 1 ? "es" : ""})`;
    return (
      <div
        className={classnames("file-result", { focused })}
        key={file.location.source.id}
      >
        <AccessibleImage className={classnames("arrow", { expanded })} />
        <AccessibleImage className="file" />
        <span className="file-path">
          {file.location.source.url
            ? getRelativePath(file.location.source.url)
            : getFormattedSourceId(file.location.source.id)}
        </span>
        <span className="matches-summary">{matches}</span>
      </div>
    );
  };

  renderMatch = (match, focused) => {
    return (
      <div
        className={classnames("result", { focused })}
        onClick={() => setTimeout(() => this.selectMatchItem(match), 50)}
      >
        <span className="line-number" key={match.location.line}>
          {match.location.line}
        </span>
        {this.highlightMatches(match)}
      </div>
    );
  };

  renderItem = (item, depth, focused, _, expanded) => {
    if (item.type === "RESULT") {
      return this.renderFile(item, focused, expanded);
    }
    return this.renderMatch(item, focused);
  };

  renderResults = () => {
    const { status, results } = this.props;
    if (!this.props.query) {
      return null;
    }
    if (results.length) {
      return (
        <Tree
          getRoots={() => results}
          getChildren={file => file.matches || []}
          itemHeight={24}
          autoExpandAll={true}
          autoExpandDepth={1}
          autoExpandNodeChildrenLimit={100}
          getParent={item => null}
          getPath={getFilePath}
          renderItem={this.renderItem}
          focused={this.state.focusedItem}
          onFocus={this.onFocus}
          isExpanded={item => {
            return this.state.expanded.has(item);
          }}
          onExpand={item => {
            const { expanded } = this.state;
            expanded.add(item);
            this.setState({ expanded });
          }}
          onCollapse={item => {
            const { expanded } = this.state;
            expanded.delete(item);
            this.setState({ expanded });
          }}
          getKey={getFilePath}
        />
      );
    }
    const msg =
      status === statusType.fetching
        ? L10N.getStr("loadingText")
        : L10N.getStr("projectTextSearch.noResults");
    return <div className="no-result-msg absolute-center">{msg}</div>;
  };

  renderSummary = () => {
    if (this.props.query !== "") {
      const resultsSummaryString = L10N.getStr("sourceSearch.resultsSummary2");
      const count = this.getResultCount();
      return PluralForm.get(count, resultsSummaryString).replace("#1", count);
    }
    return "";
  };

  shouldShowErrorEmoji() {
    return !this.getResultCount() && this.props.status === statusType.done;
  }

  renderInput() {
    const { status } = this.props;

    return (
      <SearchInput
        query={this.state.inputValue}
        count={this.getResultCount()}
        placeholder={L10N.getStr("projectTextSearch.placeholder")}
        size="small"
        showErrorEmoji={this.shouldShowErrorEmoji()}
        summaryMsg={this.renderSummary()}
        isLoading={status === statusType.fetching}
        onChange={this.inputOnChange}
        onFocus={() => this.setState({ inputFocused: true })}
        onBlur={() => this.setState({ inputFocused: false })}
        onKeyDown={this.onKeyDown}
        onHistoryScroll={this.onHistoryScroll}
        showClose={false}
        showExcludePatterns={true}
        excludePatternsLabel={L10N.getStr(
          "projectTextSearch.excludePatterns.label"
        )}
        excludePatternsPlaceholder={L10N.getStr(
          "projectTextSearch.excludePatterns.placeholder"
        )}
        ref="searchInput"
        showSearchModifiers={true}
        searchKey={searchKeys.PROJECT_SEARCH}
        onToggleSearchModifier={() => this.doSearch(this.state.inputValue)}
      />
    );
  }

  render() {
    return (
      <div className="search-container">
        <div className="project-text-search">
          <div className="header">{this.renderInput()}</div>
          {this.renderResults()}
        </div>
      </div>
    );
  }
}

ProjectSearch.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => ({
  cx: getContext(state),
  results: getProjectSearchResults(state),
  query: getProjectSearchQuery(state),
  status: getProjectSearchStatus(state),
});

export default connect(mapStateToProps, {
  searchSources: actions.searchSources,
  clearSearch: actions.clearSearch,
  selectSpecificLocation: actions.selectSpecificLocation,
  setActiveSearch: actions.setActiveSearch,
  doSearchForHighlight: actions.doSearchForHighlight,
})(ProjectSearch);
