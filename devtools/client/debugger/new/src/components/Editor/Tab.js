"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _SourceIcon = require("../shared/SourceIcon");

var _SourceIcon2 = _interopRequireDefault(_SourceIcon);

var _Button = require("../shared/Button/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _source = require("../../utils/source");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

var _clipboard = require("../../utils/clipboard");

var _tabs = require("../../utils/tabs");

var _selectors = require("../../selectors/index");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

class Tab extends _react.PureComponent {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.onTabContextMenu = (event, tab) => {
      event.preventDefault();
      this.showContextMenu(event, tab);
    }, _temp;
  }

  showContextMenu(e, tab) {
    const {
      closeTab,
      closeTabs,
      tabSources,
      showSource,
      togglePrettyPrint,
      selectedSource
    } = this.props;
    const otherTabs = tabSources.filter(t => t.get("id") !== tab);
    const sourceTab = tabSources.find(t => t.get("id") == tab);
    const tabURLs = tabSources.map(t => t.get("url"));
    const otherTabURLs = otherTabs.map(t => t.get("url"));

    if (!sourceTab) {
      return;
    }

    const isPrettySource = (0, _source.isPretty)(sourceTab);
    const tabMenuItems = (0, _tabs.getTabMenuItems)();
    const items = [{
      item: _objectSpread({}, tabMenuItems.closeTab, {
        click: () => closeTab(sourceTab.get("url"))
      })
    }, {
      item: _objectSpread({}, tabMenuItems.closeOtherTabs, {
        click: () => closeTabs(otherTabURLs)
      }),
      hidden: () => tabSources.size === 1
    }, {
      item: _objectSpread({}, tabMenuItems.closeTabsToEnd, {
        click: () => {
          const tabIndex = tabSources.findIndex(t => t.get("id") == tab);
          closeTabs(tabURLs.filter((t, i) => i > tabIndex));
        }
      }),
      hidden: () => tabSources.size === 1 || tabSources.some((t, i) => t === tab && tabSources.size - 1 === i)
    }, {
      item: _objectSpread({}, tabMenuItems.closeAllTabs, {
        click: () => closeTabs(tabURLs)
      })
    }, {
      item: {
        type: "separator"
      }
    }, {
      item: _objectSpread({}, tabMenuItems.copyToClipboard, {
        disabled: selectedSource.get("id") !== tab,
        click: () => (0, _clipboard.copyToTheClipboard)(sourceTab.text)
      })
    }, {
      item: _objectSpread({}, tabMenuItems.copySourceUri2, {
        click: () => (0, _clipboard.copyToTheClipboard)((0, _source.getRawSourceURL)(sourceTab.get("url")))
      })
    }];
    items.push({
      item: _objectSpread({}, tabMenuItems.showSource, {
        click: () => showSource(tab)
      })
    });

    if (!isPrettySource) {
      items.push({
        item: _objectSpread({}, tabMenuItems.prettyPrint, {
          click: () => togglePrettyPrint(tab)
        })
      });
    }

    (0, _devtoolsContextmenu.showMenu)(e, (0, _devtoolsContextmenu.buildMenu)(items));
  }

  isProjectSearchEnabled() {
    return this.props.activeSearch === "project";
  }

  isSourceSearchEnabled() {
    return this.props.activeSearch === "source";
  }

  render() {
    const {
      selectedSource,
      selectSpecificSource,
      closeTab,
      source
    } = this.props;
    const filename = (0, _source.getFilename)(source);
    const sourceId = source.id;
    const active = selectedSource && sourceId == selectedSource.get("id") && !this.isProjectSearchEnabled() && !this.isSourceSearchEnabled();
    const isPrettyCode = (0, _source.isPretty)(source);

    function onClickClose(e) {
      e.stopPropagation();
      closeTab(source.url);
    }

    function handleTabClick(e) {
      e.preventDefault();
      e.stopPropagation(); // Accommodate middle click to close tab

      if (e.button === 1) {
        return closeTab(source.url);
      }

      return selectSpecificSource(sourceId);
    }

    const className = (0, _classnames2.default)("source-tab", {
      active,
      pretty: isPrettyCode
    });
    return _react2.default.createElement("div", {
      className: className,
      key: sourceId,
      onMouseUp: handleTabClick,
      onContextMenu: e => this.onTabContextMenu(e, sourceId),
      title: (0, _source.getFileURL)(source)
    }, _react2.default.createElement(_SourceIcon2.default, {
      source: source,
      shouldHide: icon => ["file", "javascript"].includes(icon)
    }), _react2.default.createElement("div", {
      className: "filename"
    }, (0, _devtoolsModules.getUnicodeUrlPath)(filename)), _react2.default.createElement(_Button.CloseButton, {
      handleClick: onClickClose,
      tooltip: L10N.getStr("sourceTabs.closeTabButtonTooltip")
    }));
  }

}

const mapStateToProps = (state, {
  source
}) => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  return {
    tabSources: (0, _selectors.getSourcesForTabs)(state),
    selectedSource: selectedSource,
    activeSearch: (0, _selectors.getActiveSearch)(state)
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Tab);