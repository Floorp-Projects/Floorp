/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createFactory,
  createClass,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { getAllUi } = require("devtools/client/webconsole/new-console-output/selectors/ui");
const messagesActions = require("devtools/client/webconsole/new-console-output/actions/messages");
const uiActions = require("devtools/client/webconsole/new-console-output/actions/ui");
const { store } = require("devtools/client/webconsole/new-console-output/store");
const {
  SEVERITY_FILTER
} = require("../constants");
const FilterToggleButton = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-toggle-button").FilterToggleButton);

const FilterBar = createClass({

  displayName: "FilterBar",

  propTypes: {
    filter: PropTypes.object.isRequired,
    ui: PropTypes.object.isRequired
  },

  onClearOutputButtonClick: function () {
    store.dispatch(messagesActions.messagesClear());
  },

  onToggleFilterConfigBarButtonClick: function () {
    store.dispatch(uiActions.filterBarToggle());
  },

  onClearFiltersButtonClick: function () {
    store.dispatch(messagesActions.filtersClear());
  },

  onSearchInput: function (e) {
    store.dispatch(messagesActions.messagesSearch(e.target.value));
  },

  render() {
    const {filter, ui} = this.props;
    let configFilterBarVisible = ui.configFilterBarVisible;
    let children = [];

    children.push(dom.div({className: "devtools-toolbar webconsole-filteringbar-primary"},
      dom.button({
        className: "devtools-button devtools-clear-icon",
        title: "Clear output",
        onClick: this.onClearOutputButtonClick
      }),
      dom.button({
        className: "devtools-button devtools-filter-icon" + (
          configFilterBarVisible ? " active" : ""),
        title: "Toggle filter bar",
        onClick: this.onToggleFilterConfigBarButtonClick
      }),
      dom.input({
        className: "devtools-searchinput",
        type: "search",
        value: filter.searchText,
        placeholder: "Filter output",
        onInput: this.onSearchInput
      })
    ));

    if (configFilterBarVisible) {
      children.push(
        dom.div({className: "devtools-toolbar"},
          FilterToggleButton({
            active: filter.error,
            label: "Errors",
            filterType: SEVERITY_FILTER,
            filterKey: "error"}),
          FilterToggleButton({
            active: filter.warn,
            label: "Warnings",
            filterType: SEVERITY_FILTER,
            filterKey: "warn"}),
          FilterToggleButton({
            active: filter.log,
            label: "Logs",
            filterType: SEVERITY_FILTER,
            filterKey: "log"}),
          FilterToggleButton({
            active: filter.info,
            label: "Info",
            filterType: SEVERITY_FILTER,
            filterKey: "info"})
        )
      );
    }

    if (ui.filteredMessageVisible) {
      children.push(
        dom.div({className: "devtools-toolbar"},
          dom.span({
            className: "clear"},
            "You have filters set that may hide some results. " +
            "Learn more about our filtering syntax ",
            dom.a({}, "here"),
            "."),
          dom.button({
            className: "menu-filter-button",
            onClick: this.onClearFiltersButtonClick
          }, "Remove filters")
        )
      );
    }

    return (
      dom.div({className: "webconsole-filteringbar-wrapper"},
        children
      )
    );
  }
});

function mapStateToProps(state) {
  return {
    filter: getAllFilters(state),
    ui: getAllUi(state)
  };
}

module.exports = connect(mapStateToProps)(FilterBar);
