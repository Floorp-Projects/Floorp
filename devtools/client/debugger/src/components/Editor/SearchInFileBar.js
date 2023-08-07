/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { Component } from "react";
import { div } from "react-dom-factories";
import { connect } from "../../utils/connect";
import actions from "../../actions";
import {
  getActiveSearch,
  getSelectedSource,
  getSelectedSourceTextContent,
  getSearchOptions,
} from "../../selectors";

import { searchKeys } from "../../constants";
import { scrollList } from "../../utils/result-list";

import SearchInput from "../shared/SearchInput";
import "./SearchInFileBar.css";

const { PluralForm } = require("devtools/shared/plural-form");
const { debounce } = require("devtools/shared/debounce");
import { renderWasmText } from "../../utils/wasm";
import {
  clearSearch,
  find,
  findNext,
  findPrev,
  removeOverlay,
} from "../../utils/editor";
import { isFulfilled } from "../../utils/async-value";

function getSearchShortcut() {
  return L10N.getStr("sourceSearch.search.key2");
}

class SearchInFileBar extends Component {
  constructor(props) {
    super(props);
    this.state = {
      query: "",
      selectedResultIndex: 0,
      results: {
        matches: [],
        matchIndex: -1,
        count: 0,
        index: -1,
      },
      inputFocused: false,
    };
  }

  static get propTypes() {
    return {
      closeFileSearch: PropTypes.func.isRequired,
      editor: PropTypes.object,
      modifiers: PropTypes.object.isRequired,
      searchInFileEnabled: PropTypes.bool.isRequired,
      selectedSourceTextContent: PropTypes.bool.isRequired,
      selectedSource: PropTypes.object.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
      querySearchWorker: PropTypes.func.isRequired,
    };
  }

  componentWillUnmount() {
    const { shortcuts } = this.context;

    shortcuts.off(getSearchShortcut(), this.toggleSearch);
    shortcuts.off("Escape", this.onEscape);

    this.doSearch.cancel();
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { query } = this.state;
    // If a new source is selected update the file search results
    if (
      this.props.selectedSource &&
      nextProps.selectedSource !== this.props.selectedSource &&
      this.props.searchInFileEnabled &&
      query
    ) {
      this.doSearch(query, false);
    }
  }

  componentDidMount() {
    // overwrite this.doSearch with debounced version to
    // reduce frequency of queries
    this.doSearch = debounce(this.doSearch, 100);
    const { shortcuts } = this.context;

    shortcuts.on(getSearchShortcut(), this.toggleSearch);
    shortcuts.on("Escape", this.onEscape);
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.refs.resultList && this.refs.resultList.refs) {
      scrollList(this.refs.resultList.refs, this.state.selectedResultIndex);
    }
  }

  onEscape = e => {
    this.closeSearch(e);
  };

  clearSearch = () => {
    const { editor: ed } = this.props;
    if (ed) {
      const ctx = { ed, cm: ed.codeMirror };
      removeOverlay(ctx, this.state.query);
    }
  };

  closeSearch = e => {
    const { closeFileSearch, editor, searchInFileEnabled } = this.props;
    this.clearSearch();
    if (editor && searchInFileEnabled) {
      closeFileSearch();
      e.stopPropagation();
      e.preventDefault();
    }
    this.setState({ inputFocused: false });
  };

  toggleSearch = e => {
    e.stopPropagation();
    e.preventDefault();
    const { editor, searchInFileEnabled, setActiveSearch } = this.props;

    // Set inputFocused to false, so that search query is highlighted whenever search shortcut is used, even if the input already has focus.
    this.setState({ inputFocused: false });

    if (!searchInFileEnabled) {
      setActiveSearch("file");
    }

    if (searchInFileEnabled && editor) {
      const query = editor.codeMirror.getSelection() || this.state.query;

      if (query !== "") {
        this.setState({ query, inputFocused: true });
        this.doSearch(query);
      } else {
        this.setState({ query: "", inputFocused: true });
      }
    }
  };

  doSearch = async (query, focusFirstResult = true) => {
    const { editor, modifiers, selectedSourceTextContent } = this.props;
    if (
      !editor ||
      !selectedSourceTextContent ||
      !isFulfilled(selectedSourceTextContent) ||
      !modifiers
    ) {
      return;
    }
    const selectedContent = selectedSourceTextContent.value;

    const ctx = { ed: editor, cm: editor.codeMirror };

    if (!query) {
      clearSearch(ctx.cm, query);
      return;
    }

    let text;
    if (selectedContent.type === "wasm") {
      text = renderWasmText(this.props.selectedSource.id, selectedContent).join(
        "\n"
      );
    } else {
      text = selectedContent.value;
    }

    const matches = await this.props.querySearchWorker(query, text, modifiers);

    const res = find(ctx, query, true, modifiers, focusFirstResult);
    if (!res) {
      return;
    }

    const { ch, line } = res;

    const matchIndex = matches.findIndex(
      elm => elm.line === line && elm.ch === ch
    );
    this.setState({
      results: {
        matches,
        matchIndex,
        count: matches.length,
        index: ch,
      },
    });
  };

  traverseResults = (e, reverse = false) => {
    e.stopPropagation();
    e.preventDefault();
    const { editor } = this.props;

    if (!editor) {
      return;
    }

    const ctx = { ed: editor, cm: editor.codeMirror };

    const { modifiers } = this.props;
    const { query } = this.state;
    const { matches } = this.state.results;

    if (query === "" && !this.props.searchInFileEnabled) {
      this.props.setActiveSearch("file");
    }

    if (modifiers) {
      const findArgs = [ctx, query, true, modifiers];
      const results = reverse ? findPrev(...findArgs) : findNext(...findArgs);

      if (!results) {
        return;
      }
      const { ch, line } = results;
      const matchIndex = matches.findIndex(
        elm => elm.line === line && elm.ch === ch
      );
      this.setState({
        results: {
          matches,
          matchIndex,
          count: matches.length,
          index: ch,
        },
      });
    }
  };

  // Handlers

  onChange = e => {
    this.setState({ query: e.target.value });

    return this.doSearch(e.target.value);
  };

  onFocus = e => {
    this.setState({ inputFocused: true });
  };

  onBlur = e => {
    this.setState({ inputFocused: false });
  };

  onKeyDown = e => {
    if (e.key !== "Enter" && e.key !== "F3") {
      return;
    }

    this.traverseResults(e, e.shiftKey);
    e.preventDefault();
    this.doSearch(e.target.value);
  };

  onHistoryScroll = query => {
    this.setState({ query });
    this.doSearch(query);
  };

  // Renderers
  buildSummaryMsg() {
    const {
      query,
      results: { matchIndex, count, index },
    } = this.state;

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

  shouldShowErrorEmoji() {
    const {
      query,
      results: { count },
    } = this.state;
    return !!query && !count;
  }

  render() {
    const { searchInFileEnabled } = this.props;
    const {
      results: { count },
    } = this.state;

    if (!searchInFileEnabled) {
      return div(null);
    }
    return div(
      {
        className: "search-bar",
      },
      React.createElement(SearchInput, {
        query: this.state.query,
        count: count,
        placeholder: L10N.getStr("sourceSearch.search.placeholder2"),
        summaryMsg: this.buildSummaryMsg(),
        isLoading: false,
        onChange: this.onChange,
        onFocus: this.onFocus,
        onBlur: this.onBlur,
        showErrorEmoji: this.shouldShowErrorEmoji(),
        onKeyDown: this.onKeyDown,
        onHistoryScroll: this.onHistoryScroll,
        handleNext: e => this.traverseResults(e, false),
        handlePrev: e => this.traverseResults(e, true),
        shouldFocus: this.state.inputFocused,
        showClose: true,
        showExcludePatterns: false,
        handleClose: this.closeSearch,
        showSearchModifiers: true,
        searchKey: searchKeys.FILE_SEARCH,
        onToggleSearchModifier: () => this.doSearch(this.state.query),
      })
    );
  }
}

SearchInFileBar.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = (state, p) => {
  const selectedSource = getSelectedSource(state);

  return {
    searchInFileEnabled: getActiveSearch(state) === "file",
    selectedSource,
    selectedSourceTextContent: getSelectedSourceTextContent(state),
    modifiers: getSearchOptions(state, "file-search"),
  };
};

export default connect(mapStateToProps, {
  setFileSearchQuery: actions.setFileSearchQuery,
  setActiveSearch: actions.setActiveSearch,
  closeFileSearch: actions.closeFileSearch,
  querySearchWorker: actions.querySearchWorker,
})(SearchInFileBar);
