/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { getAllFilters } = require("devtools/client/webconsole/selectors/filters");
const { getFilteredMessagesCount } = require("devtools/client/webconsole/selectors/messages");
const { getAllUi } = require("devtools/client/webconsole/selectors/ui");
const actions = require("devtools/client/webconsole/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const { PluralForm } = require("devtools/shared/plural-form");
const {
  DEFAULT_FILTERS,
  FILTERS,
} = require("../constants");

const FilterButton = require("devtools/client/webconsole/components/FilterButton");
const FilterCheckbox = require("devtools/client/webconsole/components/FilterCheckbox");

loader.lazyRequireGetter(this, "PropTypes", "devtools/client/shared/vendor/react-prop-types");

class FilterBar extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      filter: PropTypes.object.isRequired,
      attachRefToWebConsoleUI: PropTypes.func.isRequired,
      persistLogs: PropTypes.bool.isRequired,
      hidePersistLogsCheckbox: PropTypes.bool.isRequired,
      showContentMessages: PropTypes.bool.isRequired,
      hideShowContentMessagesCheckbox: PropTypes.bool.isRequired,
      filteredMessagesCount: PropTypes.object.isRequired,
      closeButtonVisible: PropTypes.bool,
      closeSplitConsole: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      hidePersistLogsCheckbox: false,
      hideShowContentMessagesCheckbox: true,
    };
  }

  constructor(props) {
    super(props);
    this.onClickMessagesClear = this.onClickMessagesClear.bind(this);
    this.onClickRemoveAllFilters = this.onClickRemoveAllFilters.bind(this);
    this.onClickRemoveTextFilter = this.onClickRemoveTextFilter.bind(this);
    this.onSearchInput = this.onSearchInput.bind(this);
    this.onChangePersistToggle = this.onChangePersistToggle.bind(this);
    this.onChangeShowContent = this.onChangeShowContent.bind(this);
    this.renderFiltersConfigBar = this.renderFiltersConfigBar.bind(this);
    this.renderFilteredMessagesBar = this.renderFilteredMessagesBar.bind(this);
  }

  componentDidMount() {
    this.props.attachRefToWebConsoleUI(
      "filterBox",
      this.wrapperNode.querySelector(".text-filter")
    );
    this.props.attachRefToWebConsoleUI(
      "clearButton",
      this.wrapperNode.querySelector(".clear-button")
    );
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      filter,
      persistLogs,
      showContentMessages,
      filteredMessagesCount,
      closeButtonVisible,
    } = this.props;

    if (nextProps.filter !== filter) {
      return true;
    }

    if (nextProps.persistLogs !== persistLogs) {
      return true;
    }

    if (nextProps.showContentMessages !== showContentMessages) {
      return true;
    }

    if (
      JSON.stringify(nextProps.filteredMessagesCount) !==
        JSON.stringify(filteredMessagesCount)
    ) {
      return true;
    }

    if (nextProps.closeButtonVisible != closeButtonVisible) {
      return true;
    }

    return false;
  }

  onClickMessagesClear() {
    this.props.dispatch(actions.messagesClear());
  }

  onClickRemoveAllFilters() {
    this.props.dispatch(actions.defaultFiltersReset());
  }

  onClickRemoveTextFilter() {
    this.props.dispatch(actions.filterTextSet(""));
  }

  onSearchInput(e) {
    this.props.dispatch(actions.filterTextSet(e.target.value));
  }

  onChangePersistToggle() {
    this.props.dispatch(actions.persistToggle());
  }

  onChangeShowContent() {
    this.props.dispatch(actions.contentMessagesToggle());
  }

  renderFiltersConfigBar() {
    const {
      dispatch,
      filter,
      filteredMessagesCount,
    } = this.props;

    const getLabel = (baseLabel, filterKey) => {
      const count = filteredMessagesCount[filterKey];
      if (filter[filterKey] || count === 0) {
        return baseLabel;
      }
      return `${baseLabel} (${count})`;
    };

    return dom.div({
      className: "devtools-toolbar webconsole-filterbar-secondary",
      key: "config-bar",
    },
      FilterButton({
        active: filter[FILTERS.ERROR],
        label: getLabel(
          l10n.getStr("webconsole.errorsFilterButton.label"),
          FILTERS.ERROR
        ),
        filterKey: FILTERS.ERROR,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.WARN],
        label: getLabel(
          l10n.getStr("webconsole.warningsFilterButton.label"),
          FILTERS.WARN
        ),
        filterKey: FILTERS.WARN,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.LOG],
        label: getLabel(l10n.getStr("webconsole.logsFilterButton.label"), FILTERS.LOG),
        filterKey: FILTERS.LOG,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.INFO],
        label: getLabel(l10n.getStr("webconsole.infoFilterButton.label"), FILTERS.INFO),
        filterKey: FILTERS.INFO,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.DEBUG],
        label: getLabel(l10n.getStr("webconsole.debugFilterButton.label"), FILTERS.DEBUG),
        filterKey: FILTERS.DEBUG,
        dispatch,
      }),
      dom.div({
        className: "devtools-separator",
      }),
      FilterButton({
        active: filter[FILTERS.CSS],
        label: l10n.getStr("webconsole.cssFilterButton.label"),
        filterKey: FILTERS.CSS,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.NETXHR],
        label: l10n.getStr("webconsole.xhrFilterButton.label"),
        filterKey: FILTERS.NETXHR,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.NET],
        label: l10n.getStr("webconsole.requestsFilterButton.label"),
        filterKey: FILTERS.NET,
        dispatch,
      }),
    );
  }

  renderFilteredMessagesBar() {
    const {
      filteredMessagesCount,
    } = this.props;
    const {
      global,
    } = filteredMessagesCount;

    let label = l10n.getStr("webconsole.filteredMessages.label");
    label = PluralForm.get(global, label).replace("#1", global);

    // Include all default filters that are hiding messages.
    const title = DEFAULT_FILTERS.reduce((res, filter) => {
      if (filteredMessagesCount[filter] > 0) {
        return res.concat(`${filter}: ${filteredMessagesCount[filter]}`);
      }
      return res;
    }, []).join(", ");

    return dom.div({
      className: "devtools-toolbar webconsole-filterbar-filtered-messages",
      key: "filtered-messages-bar",
    },
      dom.span({
        className: "filter-message-text",
        title,
      }, label),
      dom.button({
        className: "devtools-button reset-filters-button",
        onClick: this.onClickRemoveAllFilters,
      }, l10n.getStr("webconsole.resetFiltersButton.label"))
    );
  }

  render() {
    const {
      filter,
      persistLogs,
      filteredMessagesCount,
      hidePersistLogsCheckbox,
      hideShowContentMessagesCheckbox,
      closeSplitConsole,
      showContentMessages,
    } = this.props;

    const children = [
      dom.div({
        className: "devtools-toolbar devtools-input-toolbar webconsole-filterbar-primary",
        key: "primary-bar",
      },
        dom.button({
          className: "devtools-button devtools-clear-icon",
          title: l10n.getStr("webconsole.clearButton.tooltip"),
          onClick: this.onClickMessagesClear,
        }),
        dom.div({
          className: "devtools-separator",
        }),
        dom.div(
          { className: "devtools-searchbox" },
          dom.input({
            className: "devtools-filterinput text-filter",
            type: "search",
            value: filter.text,
            placeholder: l10n.getStr("webconsole.filterInput.placeholder"),
            onInput: this.onSearchInput,
          }),
          dom.button({
            className: "devtools-searchinput-clear clear-button",
            hidden: filter.text == "",
            onClick: this.onClickRemoveTextFilter,
          }),
        ),
        !hidePersistLogsCheckbox && FilterCheckbox({
          label: l10n.getStr("webconsole.enablePersistentLogs.label"),
          title: l10n.getStr("webconsole.enablePersistentLogs.tooltip"),
          onChange: this.onChangePersistToggle,
          checked: persistLogs,
        }),
        !hideShowContentMessagesCheckbox && FilterCheckbox({
          label: l10n.getStr("browserconsole.contentMessagesCheckbox.label"),
          title: l10n.getStr("browserconsole.contentMessagesCheckbox.tooltip"),
          onChange: this.onChangeShowContent,
          checked: showContentMessages,
        }),
      ),
    ];

    if (filteredMessagesCount.global > 0) {
      children.push(this.renderFilteredMessagesBar());
    }

    if (this.props.closeButtonVisible) {
      children.push(dom.div(
        {
          className: "devtools-toolbar split-console-close-button-wrapper",
        },
        dom.button({
          id: "split-console-close-button",
          className: "devtools-button",
          title: l10n.getStr("webconsole.closeSplitConsoleButton.tooltip"),
          onClick: () => {
            closeSplitConsole();
          },
        })
      ));
    }

    children.push(this.renderFiltersConfigBar());

    return (
      dom.div({
        className: "webconsole-filteringbar-wrapper",
        "aria-live": "off",
        ref: node => {
          this.wrapperNode = node;
        },
      }, ...children
      )
    );
  }
}

function mapStateToProps(state) {
  const uiState = getAllUi(state);
  return {
    filter: getAllFilters(state),
    persistLogs: uiState.persistLogs,
    showContentMessages: uiState.showContentMessages,
    filteredMessagesCount: getFilteredMessagesCount(state),
    closeButtonVisible: uiState.closeButtonVisible,
  };
}

module.exports = connect(mapStateToProps)(FilterBar);
