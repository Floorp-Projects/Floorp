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
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const TreeView = createFactory(TreeViewClass);
const LabelCell = createFactory(
  require("devtools/client/shared/components/tree/LabelCell")
);
const { SearchProvider } = require("./search-provider");
const Toolbar = createFactory(require("./Toolbar"));

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
      connector: PropTypes.object.isRequired,
      addSearchQuery: PropTypes.func.isRequired,
      query: PropTypes.string.isRequired,
      results: PropTypes.array,
    };
  }

  constructor(props) {
    super(props);

    this.searchboxRef = createRef();
    this.renderValue = this.renderValue.bind(this);
    this.renderLabel = this.renderLabel.bind(this);

    this.provider = SearchProvider;
  }

  /**
   * Custom TreeView label rendering. The search result
   * value isn't rendered in separate column, but in the
   * same column as the label (to save space).
   */
  renderLabel(props) {
    const member = props.member;
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
    });
  }

  /**
   * Custom tree value rendering. This method is responsible for
   * rendering highlighted query string within the search result
   * result tree.
   */
  renderValue(props) {
    const member = props.member;

    // Handle only second level (zero based) that displays
    // the search result. Find the query string inside the
    // search result value (`props.object`) and render it
    // within a span element with proper class name.
    // level 0 = resource name
    if (member.level === SEARCH_RESULT_LEVEL) {
      const { query } = this.props;
      const value = member.object.value;
      const indexStart = value.indexOf(query);
      const indexEnd = indexStart + query.length;

      return span(
        {},
        span({}, value.substring(0, indexStart)),
        span({ className: "query-match" }, query),
        span({}, value.substring(indexEnd, value.length))
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
      )
    );
  }
}

module.exports = connect(
  state => ({
    query: state.search.query,
    results: state.search.results,
    ongoingSearch: state.search.ongoingSearch,
    status: state.search.status,
  }),
  dispatch => ({
    closeSearch: () => dispatch(Actions.closeSearch()),
    openSearch: () => dispatch(Actions.openSearch()),
    search: () => dispatch(Actions.search()),
    clearSearchResults: () => dispatch(Actions.clearSearchResults()),
    addSearchQuery: query => dispatch(Actions.addSearchQuery(query)),
  })
)(SearchPanel);
