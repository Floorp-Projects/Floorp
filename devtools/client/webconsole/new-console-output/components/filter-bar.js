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
const { filterTextSet, filtersClear } = require("devtools/client/webconsole/new-console-output/actions/filters");
const { messagesClear } = require("devtools/client/webconsole/new-console-output/actions/messages");
const uiActions = require("devtools/client/webconsole/new-console-output/actions/ui");
const {
  MESSAGE_LEVEL
} = require("../constants");
const FilterButton = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-button").FilterButton);

const FilterBar = createClass({

  displayName: "FilterBar",

  propTypes: {
    filter: PropTypes.object.isRequired,
    ui: PropTypes.object.isRequired
  },

  onClickMessagesClear: function () {
    this.props.dispatch(messagesClear());
  },

  onClickFilterBarToggle: function () {
    this.props.dispatch(uiActions.filterBarToggle());
  },

  onClickFiltersClear: function () {
    this.props.dispatch(filtersClear());
  },

  onSearchInput: function (e) {
    this.props.dispatch(filterTextSet(e.target.value));
  },

  render() {
    const {dispatch, filter, ui} = this.props;
    let filterBarVisible = ui.filterBarVisible;
    let children = [];

    children.push(dom.div({className: "devtools-toolbar webconsole-filterbar-primary"},
      dom.button({
        className: "devtools-button devtools-clear-icon",
        title: "Clear output",
        onClick: this.onClickMessagesClear
      }),
      dom.button({
        className: "devtools-button devtools-filter-icon" + (
          filterBarVisible ? " checked" : ""),
        title: "Toggle filter bar",
        onClick: this.onClickFilterBarToggle
      }),
      dom.input({
        className: "devtools-plaininput",
        type: "search",
        value: filter.text,
        placeholder: "Filter output",
        onInput: this.onSearchInput
      })
    ));

    if (filterBarVisible) {
      children.push(
        dom.div({className: "devtools-toolbar"},
          FilterButton({
            active: filter.error,
            label: "Errors",
            filterKey: MESSAGE_LEVEL.ERROR,
            dispatch
          }),
          FilterButton({
            active: filter.warn,
            label: "Warnings",
            filterKey: MESSAGE_LEVEL.WARN,
            dispatch
          }),
          FilterButton({
            active: filter.log,
            label: "Logs",
            filterKey: MESSAGE_LEVEL.LOG,
            dispatch
          }),
          FilterButton({
            active: filter.info,
            label: "Info",
            filterKey: MESSAGE_LEVEL.INFO,
            dispatch
          })
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
            onClick: this.onClickFiltersClear
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
