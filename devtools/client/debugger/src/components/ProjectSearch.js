/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import PropTypes from "prop-types";
import React, { Component } from "react";
import { connect } from "../utils/connect";
import classnames from "classnames";
import actions from "../actions";

import { getEditor } from "../utils/editor";
import { highlightMatches } from "../utils/project-search";

import { statusType } from "../reducers/project-text-search";
import { getRelativePath } from "../utils/sources-tree";
import {
  getActiveSearch,
  getTextSearchResults,
  getTextSearchStatus,
  getTextSearchQuery,
  getContext,
} from "../selectors";

import ManagedTree from "./shared/ManagedTree";
import SearchInput from "./shared/SearchInput";
import AccessibleImage from "./shared/AccessibleImage";

import type { List } from "immutable";
import type { ActiveSearchType } from "../reducers/types";
import type { StatusType } from "../reducers/project-text-search";
import type { Context, SourceId } from "../types";
import { PluralForm } from "devtools-modules";

import "./ProjectSearch.css";

export type Match = {
  type: "MATCH",
  sourceId: SourceId,
  line: number,
  column: number,
  matchIndex: number,
  match: string,
  value: string,
  text: string,
};

type Result = {
  type: "RESULT",
  filepath: string,
  matches: Array<Match>,
  sourceId: SourceId,
};

type Item = Result | Match;

type State = {
  inputValue: string,
  inputFocused: boolean,
  focusedItem: ?Item,
};

type OwnProps = {||};
type Props = {
  cx: Context,
  query: string,
  results: List<Result>,
  status: StatusType,
  activeSearch: ?ActiveSearchType,
  closeProjectSearch: typeof actions.closeProjectSearch,
  searchSources: typeof actions.searchSources,
  clearSearch: typeof actions.clearSearch,
  selectSpecificLocation: typeof actions.selectSpecificLocation,
  setActiveSearch: typeof actions.setActiveSearch,
  doSearchForHighlight: typeof actions.doSearchForHighlight,
};

function getFilePath(item: Item, index?: number) {
  return item.type === "RESULT"
    ? `${item.sourceId}-${index || "$"}`
    : `${item.sourceId}-${item.line}-${item.column}-${index || "$"}`;
}

function sanitizeQuery(query: string): string {
  // no '\' at end of query
  return query.replace(/\\$/, "");
}

export class ProjectSearch extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = {
      inputValue: this.props.query || "",
      inputFocused: false,
      focusedItem: null,
    };
  }

  componentDidMount() {
    const { shortcuts } = this.context;

    shortcuts.on(
      L10N.getStr("projectTextSearch.key"),
      this.toggleProjectTextSearch
    );
    shortcuts.on("Enter", this.onEnterPress);
  }

  componentWillUnmount() {
    const { shortcuts } = this.context;
    shortcuts.off(
      L10N.getStr("projectTextSearch.key"),
      this.toggleProjectTextSearch
    );
    shortcuts.off("Enter", this.onEnterPress);
  }

  componentDidUpdate(prevProps: Props) {
    // If the query changes in redux, also change it in the UI
    if (prevProps.query !== this.props.query) {
      this.setState({ inputValue: this.props.query });
    }
  }

  doSearch(searchTerm: string) {
    this.props.searchSources(this.props.cx, searchTerm);
  }

  toggleProjectTextSearch = (key: string, e: KeyboardEvent) => {
    const { cx, closeProjectSearch, setActiveSearch } = this.props;
    if (e) {
      e.preventDefault();
    }

    if (this.isProjectSearchEnabled()) {
      return closeProjectSearch(cx);
    }

    return setActiveSearch("project");
  };

  isProjectSearchEnabled = () => this.props.activeSearch === "project";

  selectMatchItem = (matchItem: Match) => {
    this.props.selectSpecificLocation(this.props.cx, {
      sourceId: matchItem.sourceId,
      line: matchItem.line,
      column: matchItem.column,
    });
    this.props.doSearchForHighlight(
      this.state.inputValue,
      getEditor(),
      matchItem.line,
      matchItem.column
    );
  };

  getResultCount = () =>
    this.props.results.reduce((count, file) => count + file.matches.length, 0);

  onKeyDown = (e: SyntheticKeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Escape") {
      return;
    }

    e.stopPropagation();

    if (e.key !== "Enter") {
      return;
    }
    this.setState({ focusedItem: null });
    const query = sanitizeQuery(this.state.inputValue);
    if (query) {
      this.doSearch(query);
    }
  };

  onHistoryScroll = (query: string) => {
    this.setState({ inputValue: query });
  };

  onEnterPress = () => {
    if (
      !this.isProjectSearchEnabled() ||
      !this.state.focusedItem ||
      this.state.inputFocused
    ) {
      return;
    }
    if (this.state.focusedItem.type === "MATCH") {
      this.selectMatchItem(this.state.focusedItem);
    }
  };

  onFocus = (item: Item) => {
    if (this.state.focusedItem !== item) {
      this.setState({ focusedItem: item });
    }
  };

  inputOnChange = (e: SyntheticInputEvent<HTMLInputElement>) => {
    const inputValue = e.target.value;
    const { cx, clearSearch } = this.props;
    this.setState({ inputValue });
    if (inputValue === "") {
      clearSearch(cx);
    }
  };

  renderFile = (file: Result, focused: boolean, expanded: boolean) => {
    const matchesLength = file.matches.length;
    const matches = ` (${matchesLength} match${matchesLength > 1 ? "es" : ""})`;

    return (
      <div
        className={classnames("file-result", { focused })}
        key={file.sourceId}
      >
        <AccessibleImage className={classnames("arrow", { expanded })} />
        <AccessibleImage className="file" />
        <span className="file-path">{getRelativePath(file.filepath)}</span>
        <span className="matches-summary">{matches}</span>
      </div>
    );
  };

  renderMatch = (match: Match, focused: boolean) => {
    return (
      <div
        className={classnames("result", { focused })}
        onClick={() => setTimeout(() => this.selectMatchItem(match), 50)}
      >
        <span className="line-number" key={match.line}>
          {match.line}
        </span>
        {highlightMatches(match)}
      </div>
    );
  };

  renderItem = (
    item: Item,
    depth: number,
    focused: boolean,
    _: any,
    expanded: boolean
  ) => {
    if (item.type === "RESULT") {
      return this.renderFile(item, focused, expanded);
    }
    return this.renderMatch(item, focused);
  };

  renderResults = () => {
    const { status, results } = this.props;
    if (!this.props.query) {
      return;
    }
    if (results.length) {
      return (
        <ManagedTree
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
    const { cx, closeProjectSearch, status } = this.props;
    return (
      <SearchInput
        query={this.state.inputValue}
        count={this.getResultCount()}
        placeholder={L10N.getStr("projectTextSearch.placeholder")}
        size="big"
        showErrorEmoji={this.shouldShowErrorEmoji()}
        summaryMsg={this.renderSummary()}
        isLoading={status === statusType.fetching}
        onChange={this.inputOnChange}
        onFocus={() => this.setState({ inputFocused: true })}
        onBlur={() => this.setState({ inputFocused: false })}
        onKeyDown={this.onKeyDown}
        onHistoryScroll={this.onHistoryScroll}
        handleClose={() => closeProjectSearch(cx)}
        ref="searchInput"
      />
    );
  }

  render() {
    if (!this.isProjectSearchEnabled()) {
      return null;
    }

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
  activeSearch: getActiveSearch(state),
  results: getTextSearchResults(state),
  query: getTextSearchQuery(state),
  status: getTextSearchStatus(state),
});

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps, {
  closeProjectSearch: actions.closeProjectSearch,
  searchSources: actions.searchSources,
  clearSearch: actions.clearSearch,
  selectSpecificLocation: actions.selectSpecificLocation,
  setActiveSearch: actions.setActiveSearch,
  doSearchForHighlight: actions.doSearchForHighlight,
})(ProjectSearch);
