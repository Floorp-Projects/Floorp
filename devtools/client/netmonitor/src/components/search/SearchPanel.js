/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createRef,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div, span } = dom;
const Actions = require("devtools/client/netmonitor/src/actions/index");
const { PANELS } = require("devtools/client/netmonitor/src/constants");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const TreeView = createFactory(TreeViewClass);
const LabelCell = createFactory(
  require("devtools/client/shared/components/tree/LabelCell")
);
const {
  SearchProvider,
} = require("devtools/client/netmonitor/src/components/search/search-provider");
const Toolbar = createFactory(
  require("devtools/client/netmonitor/src/components/search/Toolbar")
);
const StatusBar = createFactory(
  require("devtools/client/netmonitor/src/components/search/StatusBar")
);
const {
  limitTooltipLength,
} = require("devtools/client/netmonitor/src/utils/tooltips");
// There are two levels in the search panel tree hierarchy:
// 0: Resource - represents the source request object
// 1: Search Result - represents a match coming from the parent resource
const RESOURCE_LEVEL = 0;
const SEARCH_RESULT_LEVEL = 1;

/**
 * This component is responsible for rendering all search results
 * coming from the current search.
 */
class SearchPanel extends Component {
  static get propTypes() {
    return {
      clearSearchResults: PropTypes.func.isRequired,
      openSearch: PropTypes.func.isRequired,
      closeSearch: PropTypes.func.isRequired,
      search: PropTypes.func.isRequired,
      caseSensitive: PropTypes.bool,
      connector: PropTypes.object.isRequired,
      addSearchQuery: PropTypes.func.isRequired,
      query: PropTypes.string.isRequired,
      results: PropTypes.array,
      navigate: PropTypes.func.isRequired,
      isDisplaying: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.searchboxRef = createRef();
    this.renderValue = this.renderValue.bind(this);
    this.renderLabel = this.renderLabel.bind(this);
    this.onClickTreeRow = this.onClickTreeRow.bind(this);
    this.provider = SearchProvider;
  }

  componentDidMount() {
    if (this.searchboxRef) {
      this.searchboxRef.current.focus();
    }
  }

  componentDidUpdate(prevProps) {
    if (this.props.isDisplaying && !prevProps.isDisplaying) {
      this.searchboxRef.current.focus();
    }
  }

  onClickTreeRow(path, event, member) {
    if (member.object.parentResource) {
      this.props.navigate(member.object);
    }
  }

  /**
   * Custom TreeView label rendering. The search result
   * value isn't rendered in separate column, but in the
   * same column as the label (to save space).
   */
  renderLabel(props) {
    const { member } = props;
    const level = member.level || 0;
    const className = level == RESOURCE_LEVEL ? "resourceCell" : "resultCell";

    // Customize label rendering by adding a suffix/value
    const renderSuffix = () => {
      return dom.span(
        {
          className,
        },
        " ",
        this.renderValue(props)
      );
    };

    return LabelCell({
      ...props,
      title:
        member.level == 1
          ? limitTooltipLength(member.object.value)
          : this.provider.getResourceTooltipLabel(member.object),
      renderSuffix,
    });
  }

  renderTree() {
    const { results } = this.props;
    return TreeView({
      object: results,
      provider: this.provider,
      expandableStrings: false,
      renderLabelCell: this.renderLabel,
      columns: [],
      onClickRow: this.onClickTreeRow,
    });
  }

  /**
   * Custom tree value rendering. This method is responsible for
   * rendering highlighted query string within the search result
   * result tree.
   */
  renderValue(props) {
    const { member } = props;
    const { query, caseSensitive } = this.props;

    // Handle only second level (zero based) that displays
    // the search result. Find the query string inside the
    // search result value (`props.object`) and render it
    // within a span element with proper class name.
    // level 0 = resource name
    if (member.level === SEARCH_RESULT_LEVEL) {
      const { object } = member;

      // Handles multiple matches in a string
      if (object.startIndex && object.startIndex.length > 1) {
        let indexStart = 0;
        const allMatches = object.startIndex.map((match, index) => {
          if (index === 0) {
            indexStart = match - 50;
          }

          const highlightedMatch = [
            span(
              { key: "match-" + match },
              object.value.substring(indexStart, match - query.length)
            ),
            span(
              {
                className: "query-match",
                key: "match-" + match + "-highlight",
              },
              object.value.substring(match - query.length, match)
            ),
          ];

          indexStart = match;

          return highlightedMatch;
        });

        return span(
          {
            title: limitTooltipLength(object.value),
          },
          allMatches
        );
      }

      const indexStart = caseSensitive
        ? object.value.indexOf(query)
        : object.value.toLowerCase().indexOf(query.toLowerCase());
      const indexEnd = indexStart + query.length;

      // Handles a match in a string
      if (indexStart >= 0) {
        return span(
          { title: limitTooltipLength(object.value) },
          span({}, object.value.substring(0, indexStart)),
          span(
            { className: "query-match" },
            object.value.substring(indexStart, indexStart + query.length)
          ),
          span({}, object.value.substring(indexEnd, object.value.length))
        );
      }

      // Default for key:value matches where query might not
      // be present in the value, but found in the key.
      return span(
        { title: limitTooltipLength(object.value) },
        span({}, object.value)
      );
    }

    return this.provider.getValue(member.object);
  }

  render() {
    const {
      openSearch,
      closeSearch,
      clearSearchResults,
      connector,
      addSearchQuery,
      search,
    } = this.props;
    return div(
      { className: "search-panel", style: { width: "100%" } },
      Toolbar({
        searchboxRef: this.searchboxRef,
        openSearch,
        closeSearch,
        clearSearchResults,
        addSearchQuery,
        search,
        connector,
      }),
      div(
        { className: "search-panel-content", style: { width: "100%" } },
        this.renderTree()
      ),
      StatusBar()
    );
  }
}

module.exports = connect(
  state => ({
    query: state.search.query,
    caseSensitive: state.search.caseSensitive,
    results: state.search.results,
    ongoingSearch: state.search.ongoingSearch,
    isDisplaying: state.ui.selectedActionBarTabId === PANELS.SEARCH,
    status: state.search.status,
  }),
  dispatch => ({
    closeSearch: () => dispatch(Actions.closeSearch()),
    openSearch: () => dispatch(Actions.openSearch()),
    search: () => dispatch(Actions.search()),
    clearSearchResults: () => dispatch(Actions.clearSearchResults()),
    addSearchQuery: query => dispatch(Actions.addSearchQuery(query)),
    navigate: searchResult => dispatch(Actions.navigate(searchResult)),
  })
)(SearchPanel);
