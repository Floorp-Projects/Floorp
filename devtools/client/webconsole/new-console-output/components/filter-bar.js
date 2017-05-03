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
const {
  filterTextSet,
  filtersClear,
  filterBarToggle,
  messagesClear
} = require("devtools/client/webconsole/new-console-output/actions/index");
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
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
    filterBarVisible: PropTypes.bool.isRequired,
  },

  componentDidMount() {
    this.props.serviceContainer.attachRefToHud("filterBox",
      this.wrapperNode.querySelector(".text-filter"));
  },

  onClickMessagesClear: function () {
    this.props.dispatch(messagesClear());
  },

  onClickFilterBarToggle: function () {
    this.props.dispatch(filterBarToggle());
  },

  onClickFiltersClear: function () {
    this.props.dispatch(filtersClear());
  },

  onSearchInput: function (e) {
    this.props.dispatch(filterTextSet(e.target.value));
  },

  render() {
    const {dispatch, filter, filterBarVisible} = this.props;
    let children = [];

    children.push(dom.div({className: "devtools-toolbar webconsole-filterbar-primary"},
      dom.button({
        className: "devtools-button devtools-clear-icon",
        title: l10n.getStr("webconsole.clearButton.tooltip"),
        onClick: this.onClickMessagesClear
      }),
      dom.button({
        className: "devtools-button devtools-filter-icon" + (
          filterBarVisible ? " checked" : ""),
        title: l10n.getStr("webconsole.toggleFilterButton.tooltip"),
        onClick: this.onClickFilterBarToggle
      }),
      dom.input({
        className: "devtools-plaininput text-filter",
        type: "search",
        value: filter.text,
        placeholder: l10n.getStr("webconsole.filterInput.placeholder"),
        onInput: this.onSearchInput
      })
    ));

    if (filterBarVisible) {
      children.push(
        dom.div({className: "devtools-toolbar webconsole-filterbar-secondary"},
          FilterButton({
            active: filter.error,
            label: l10n.getStr("webconsole.errorsFilterButton.label"),
            filterKey: MESSAGE_LEVEL.ERROR,
            dispatch
          }),
          FilterButton({
            active: filter.warn,
            label: l10n.getStr("webconsole.warningsFilterButton.label"),
            filterKey: MESSAGE_LEVEL.WARN,
            dispatch
          }),
          FilterButton({
            active: filter.log,
            label: l10n.getStr("webconsole.logsFilterButton.label"),
            filterKey: MESSAGE_LEVEL.LOG,
            dispatch
          }),
          FilterButton({
            active: filter.info,
            label: l10n.getStr("webconsole.infoFilterButton.label"),
            filterKey: MESSAGE_LEVEL.INFO,
            dispatch
          }),
          FilterButton({
            active: filter.debug,
            label: l10n.getStr("webconsole.debugFilterButton.label"),
            filterKey: MESSAGE_LEVEL.DEBUG,
            dispatch
          }),
          dom.span({
            className: "devtools-separator",
          }),
          FilterButton({
            active: filter.css,
            label: l10n.getStr("webconsole.cssFilterButton.label"),
            filterKey: "css",
            dispatch
          }),
          FilterButton({
            active: filter.netxhr,
            label: l10n.getStr("webconsole.xhrFilterButton.label"),
            filterKey: "netxhr",
            dispatch
          }),
          FilterButton({
            active: filter.net,
            label: l10n.getStr("webconsole.requestsFilterButton.label"),
            filterKey: "net",
            dispatch
          })
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
    filterBarVisible: getAllUi(state).filterBarVisible,
  };
}

module.exports = connect(mapStateToProps)(FilterBar);
