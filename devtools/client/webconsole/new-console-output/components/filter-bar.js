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
const { filterTextSet, filtersClear } = require("devtools/client/webconsole/new-console-output/actions/index");
const { messagesClear } = require("devtools/client/webconsole/new-console-output/actions/index");
const uiActions = require("devtools/client/webconsole/new-console-output/actions/index");
const {
  MESSAGE_LEVEL
} = require("../constants");
const FilterButton = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-button"));

const FilterBar = createClass({

  displayName: "FilterBar",

  propTypes: {
    dispatch: PropTypes.func.isRequired,
    filter: PropTypes.object.isRequired,
    serviceContainer: PropTypes.shape({
      attachRefToHud: PropTypes.func.isRequired,
    }).isRequired,
    ui: PropTypes.object.isRequired,
  },

  componentDidMount() {
    this.props.serviceContainer.attachRefToHud("filterBox",
      this.wrapperNode.querySelector(".text-filter"));
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
        className: "devtools-plaininput text-filter",
        type: "search",
        value: filter.text,
        placeholder: "Filter output",
        onInput: this.onSearchInput
      })
    ));

    if (filterBarVisible) {
      children.push(
        dom.div({className: "devtools-toolbar webconsole-filterbar-secondary"},
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
          }),
          FilterButton({
            active: filter.debug,
            label: "Debug",
            filterKey: MESSAGE_LEVEL.DEBUG,
            dispatch
          }),
          dom.span({
            className: "devtools-separator",
          }),
          FilterButton({
            active: filter.css,
            label: "CSS",
            filterKey: "css",
            dispatch
          }),
          FilterButton({
            active: filter.netxhr,
            label: "XHR",
            filterKey: "netxhr",
            dispatch
          }),
          FilterButton({
            active: filter.net,
            label: "Requests",
            filterKey: "net",
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
      dom.div({
        className: "webconsole-filteringbar-wrapper",
        ref: node => {
          this.wrapperNode = node;
        }
      }, ...children
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
