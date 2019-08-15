/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import PropTypes from "prop-types";
import React, { Component } from "react";
import { connect } from "../../utils/connect";
import { CloseButton } from "../shared/Button";
import AccessibleImage from "../shared/AccessibleImage";
import actions from "../../actions";
import {
  getActiveSearch,
  getSelectedSource,
  getSourceContent,
  getSelectedLocation,
  getFileSearchQuery,
  getFileSearchModifiers,
  getFileSearchResults,
  getHighlightedLineRange,
  getContext,
} from "../../selectors";

import { removeOverlay } from "../../utils/editor";

import { scrollList } from "../../utils/result-list";
import classnames from "classnames";

import type { Source, Context } from "../../types";
import type { Modifiers, SearchResults } from "../../reducers/file-search";

import SearchInput from "../shared/SearchInput";
import { debounce } from "lodash";
import "./SearchBar.css";
import { PluralForm } from "devtools-modules";

import type SourceEditor from "../../utils/editor/source-editor";

function getShortcuts() {
  const searchAgainKey = L10N.getStr("sourceSearch.search.again.key3");
  const searchAgainPrevKey = L10N.getStr("sourceSearch.search.againPrev.key3");
  const searchKey = L10N.getStr("sourceSearch.search.key2");

  return {
    shiftSearchAgainShortcut: searchAgainPrevKey,
    searchAgainShortcut: searchAgainKey,
    searchShortcut: searchKey,
  };
}

type State = {
  query: string,
  selectedResultIndex: number,
  count: number,
  index: number,
  inputFocused: boolean,
};

type Props = {
  cx: Context,
  editor: SourceEditor,
  selectedSource?: Source,
  selectedContentLoaded: boolean,
  searchOn: boolean,
  searchResults: SearchResults,
  modifiers: Modifiers,
  query: string,
  showClose?: boolean,
  size?: string,
  toggleFileSearchModifier: typeof actions.toggleFileSearchModifier,
  setFileSearchQuery: typeof actions.setFileSearchQuery,
  setActiveSearch: typeof actions.setActiveSearch,
  closeFileSearch: typeof actions.closeFileSearch,
  doSearch: typeof actions.doSearch,
  traverseResults: typeof actions.traverseResults,
};

class SearchBar extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = {
      query: props.query,
      selectedResultIndex: 0,
      count: 0,
      index: -1,
      inputFocused: false,
    };
  }

  componentWillUnmount() {
    const shortcuts = this.context.shortcuts;
    const {
      searchShortcut,
      searchAgainShortcut,
      shiftSearchAgainShortcut,
    } = getShortcuts();

    shortcuts.off(searchShortcut);
    shortcuts.off("Escape");
    shortcuts.off(searchAgainShortcut);
    shortcuts.off(shiftSearchAgainShortcut);
  }

  componentDidMount() {
    // overwrite this.doSearch with debounced version to
    // reduce frequency of queries
    this.doSearch = debounce(this.doSearch, 100);
    const shortcuts = this.context.shortcuts;
    const {
      searchShortcut,
      searchAgainShortcut,
      shiftSearchAgainShortcut,
    } = getShortcuts();

    shortcuts.on(searchShortcut, (_, e) => this.toggleSearch(e));
    shortcuts.on("Escape", (_, e) => this.onEscape(e));

    shortcuts.on(shiftSearchAgainShortcut, (_, e) =>
      this.traverseResults(e, true)
    );

    shortcuts.on(searchAgainShortcut, (_, e) => this.traverseResults(e, false));
  }

  componentDidUpdate(prevProps: Props, prevState: State) {
    if (this.refs.resultList && this.refs.resultList.refs) {
      scrollList(this.refs.resultList.refs, this.state.selectedResultIndex);
    }
  }

  onEscape = (e: SyntheticKeyboardEvent<HTMLElement>) => {
    this.closeSearch(e);
  };

  clearSearch = () => {
    const { editor: ed, query } = this.props;
    if (ed) {
      const ctx = { ed, cm: ed.codeMirror };
      removeOverlay(ctx, query);
    }
  };

  closeSearch = (e: SyntheticKeyboardEvent<HTMLElement>) => {
    const { cx, closeFileSearch, editor, searchOn, query } = this.props;
    this.clearSearch();
    if (editor && searchOn) {
      closeFileSearch(cx, editor);
      e.stopPropagation();
      e.preventDefault();
    }
    this.setState({ query, inputFocused: false });
  };

  toggleSearch = (e: SyntheticKeyboardEvent<HTMLElement>) => {
    e.stopPropagation();
    e.preventDefault();
    const { editor, searchOn, setActiveSearch } = this.props;

    // Set inputFocused to false, so that search query is highlighted whenever search shortcut is used, even if the input already has focus.
    this.setState({ inputFocused: false });

    if (!searchOn) {
      setActiveSearch("file");
    }

    if (this.props.searchOn && editor) {
      const query = editor.codeMirror.getSelection() || this.state.query;

      if (query !== "") {
        this.setState({ query, inputFocused: true });
        this.doSearch(query);
      } else {
        this.setState({ query: "", inputFocused: true });
      }
    }
  };

  doSearch = (query: string) => {
    const { cx, selectedSource, selectedContentLoaded } = this.props;
    if (!selectedSource || !selectedContentLoaded) {
      return;
    }

    this.props.doSearch(cx, query, this.props.editor);
  };

  traverseResults = (e: SyntheticEvent<HTMLElement>, rev: boolean) => {
    e.stopPropagation();
    e.preventDefault();
    const editor = this.props.editor;

    if (!editor) {
      return;
    }
    this.props.traverseResults(this.props.cx, rev, editor);
  };

  // Handlers

  onChange = (e: SyntheticInputEvent<HTMLElement>) => {
    this.setState({ query: e.target.value });

    return this.doSearch(e.target.value);
  };

  onFocus = (e: SyntheticFocusEvent<HTMLElement>) => {
    this.setState({ inputFocused: true });
  };

  onBlur = (e: SyntheticFocusEvent<HTMLElement>) => {
    this.setState({ inputFocused: false });
  };

  onKeyDown = (e: any) => {
    if (e.key !== "Enter" && e.key !== "F3") {
      return;
    }

    this.traverseResults(e, e.shiftKey);
    e.preventDefault();
    return this.doSearch(e.target.value);
  };

  onHistoryScroll = (query: string) => {
    this.setState({ query });
    this.doSearch(query);
  };

  // Renderers
  buildSummaryMsg() {
    const {
      searchResults: { matchIndex, count, index },
      query,
    } = this.props;

    if (query.trim() == "") {
      return "";
    }

    if (count == 0) {
      return L10N.getStr("editor.noResultsFound");
    }

    if (index == -1) {
      const resultsSummaryString = L10N.getStr("sourceSearch.resultsSummary1");
      return PluralForm.get(count, resultsSummaryString).replace("#1", count);
    }

    const searchResultsString = L10N.getStr("editor.searchResults1");
    return PluralForm.get(count, searchResultsString)
      .replace("#1", count)
      .replace("%d", matchIndex + 1);
  }

  renderSearchModifiers = () => {
    const { cx, modifiers, toggleFileSearchModifier, query } = this.props;
    const { doSearch } = this;

    function SearchModBtn({ modVal, className, svgName, tooltip }) {
      const preppedClass = classnames(className, {
        active: modifiers && modifiers[modVal],
      });
      return (
        <button
          className={preppedClass}
          onMouseDown={() => {
            toggleFileSearchModifier(cx, modVal);
            doSearch(query);
          }}
          onKeyDown={(e: any) => {
            if (e.key === "Enter") {
              toggleFileSearchModifier(cx, modVal);
              doSearch(query);
            }
          }}
          title={tooltip}
        >
          <AccessibleImage className={svgName} />
        </button>
      );
    }

    return (
      <div className="search-modifiers">
        <span className="pipe-divider" />
        <span className="search-type-name">
          {L10N.getStr("symbolSearch.searchModifier.modifiersLabel")}
        </span>
        <SearchModBtn
          modVal="regexMatch"
          className="regex-match-btn"
          svgName="regex-match"
          tooltip={L10N.getStr("symbolSearch.searchModifier.regex")}
        />
        <SearchModBtn
          modVal="caseSensitive"
          className="case-sensitive-btn"
          svgName="case-match"
          tooltip={L10N.getStr("symbolSearch.searchModifier.caseSensitive")}
        />
        <SearchModBtn
          modVal="wholeWord"
          className="whole-word-btn"
          svgName="whole-word-match"
          tooltip={L10N.getStr("symbolSearch.searchModifier.wholeWord")}
        />
      </div>
    );
  };

  shouldShowErrorEmoji() {
    const {
      query,
      searchResults: { count },
    } = this.props;
    return !!query && !count;
  }

  render() {
    const {
      searchResults: { count },
      searchOn,
      showClose = true,
      size = "big",
    } = this.props;

    if (!searchOn) {
      return <div />;
    }

    return (
      <div className="search-bar">
        <SearchInput
          query={this.state.query}
          count={count}
          placeholder={L10N.getStr("sourceSearch.search.placeholder2")}
          summaryMsg={this.buildSummaryMsg()}
          isLoading={false}
          onChange={this.onChange}
          onFocus={this.onFocus}
          onBlur={this.onBlur}
          showErrorEmoji={this.shouldShowErrorEmoji()}
          onKeyDown={this.onKeyDown}
          onHistoryScroll={this.onHistoryScroll}
          handleNext={e => this.traverseResults(e, false)}
          handlePrev={e => this.traverseResults(e, true)}
          shouldFocus={this.state.inputFocused}
          showClose={false}
        />
        <div className="search-bottom-bar">
          {this.renderSearchModifiers()}
          {showClose && (
            <React.Fragment>
              <span className="pipe-divider" />
              <CloseButton handleClick={this.closeSearch} buttonClass={size} />
            </React.Fragment>
          )}
        </div>
      </div>
    );
  }
}

SearchBar.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);

  return {
    cx: getContext(state),
    searchOn: getActiveSearch(state) === "file",
    selectedSource,
    selectedContentLoaded: selectedSource
      ? !!getSourceContent(state, selectedSource.id)
      : null,
    selectedLocation: getSelectedLocation(state),
    query: getFileSearchQuery(state),
    modifiers: getFileSearchModifiers(state),
    highlightedLineRange: getHighlightedLineRange(state),
    searchResults: getFileSearchResults(state),
  };
};

export default connect(
  mapStateToProps,
  {
    toggleFileSearchModifier: actions.toggleFileSearchModifier,
    setFileSearchQuery: actions.setFileSearchQuery,
    setActiveSearch: actions.setActiveSearch,
    closeFileSearch: actions.closeFileSearch,
    doSearch: actions.doSearch,
    traverseResults: actions.traverseResults,
  }
)(SearchBar);
