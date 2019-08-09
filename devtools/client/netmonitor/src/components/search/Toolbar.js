/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { FILTER_SEARCH_DELAY } = require("../../constants");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const { button, span, div } = dom;

// Components
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

/**
 * Network Search toolbar component.
 *
 * Provides tools for greater control over search.
 */
class Toolbar extends Component {
  static get propTypes() {
    return {
      searchboxRef: PropTypes.object.isRequired,
      clearSearchResults: PropTypes.func.isRequired,
      search: PropTypes.func.isRequired,
      closeSearch: PropTypes.func.isRequired,
      addSearchQuery: PropTypes.func.isRequired,
      connector: PropTypes.object.isRequired,
    };
  }

  /**
   * Render a separator.
   */
  renderSeparator() {
    return span({ className: "devtools-separator" });
  }

  /**
   * Handles what we do when key is pressed in search input.
   * @param event
   * @param conn
   */
  onKeyDown(event, connector) {
    switch (event.key) {
      case "Escape":
        event.preventDefault();
        this.props.closeSearch();
        break;
      case "Enter":
        event.preventDefault();
        this.props.addSearchQuery(event.target.value);
        this.props.search(connector, event.target.value);
        break;
    }
  }

  renderCloseButton() {
    const { closeSearch } = this.props;
    return button({
      id: "devtools-network-search-close",
      className: "devtools-button",
      title: L10N.getStr("netmonitor.search.toolbar.close"),
      onClick: () => closeSearch(),
    });
  }

  /**
   * Render a clear button to clear search results.
   */
  renderClearButton() {
    return button({
      className:
        "devtools-button devtools-clear-icon ws-frames-list-clear-button",
      title: L10N.getStr("netmonitor.search.toolbar.clear"),
      onClick: () => {
        this.props.clearSearchResults();
      },
    });
  }

  /**
   * Render filter Search box.
   */
  renderFilterBox() {
    const { addSearchQuery, connector } = this.props;
    return SearchBox({
      keyShortcut: "CmdOrCtrl+Shift+F",
      placeholder: L10N.getStr("netmonitor.search.toolbar.inputPlaceholder"),
      type: "search",
      delay: FILTER_SEARCH_DELAY,
      ref: this.props.searchboxRef,
      onChange: query => addSearchQuery(query),
      onKeyDown: event => this.onKeyDown(event, connector),
    });
  }

  render() {
    return div(
      {
        id: "netmonitor-toolbar-container",
        className: "devtools-toolbar devtools-input-toolbar",
      },
      this.renderClearButton(),
      this.renderSeparator(),
      this.renderFilterBox(),
      this.renderCloseButton()
    );
  }
}

module.exports = connect(
  state => ({}),
  dispatch => ({
    closeSearch: () => dispatch(Actions.closeSearch()),
    openSearch: () => dispatch(Actions.openSearch()),
    clearSearchResults: () => dispatch(Actions.clearSearchResults()),
    search: (connector, query) => dispatch(Actions.search(connector, query)),
    addSearchQuery: query => dispatch(Actions.addSearchQuery(query)),
  })
)(Toolbar);
