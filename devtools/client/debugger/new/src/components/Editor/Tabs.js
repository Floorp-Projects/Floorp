"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _selectors = require("../../selectors/index");

var _ui = require("../../utils/ui");

var _tabs = require("../../utils/tabs");

var _source = require("../../utils/source");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _lodash = require("devtools/client/shared/vendor/lodash");

var _Tab = require("./Tab");

var _Tab2 = _interopRequireDefault(_Tab);

var _PaneToggle = require("../shared/Button/PaneToggle");

var _PaneToggle2 = _interopRequireDefault(_PaneToggle);

var _Dropdown = require("../shared/Dropdown");

var _Dropdown2 = _interopRequireDefault(_Dropdown);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Tabs extends _react.PureComponent {
  constructor(props) {
    super(props);

    this.updateHiddenTabs = () => {
      if (!this.refs.sourceTabs) {
        return;
      }

      const {
        selectedSource,
        tabSources,
        moveTab
      } = this.props;
      const sourceTabEls = this.refs.sourceTabs.children;
      const hiddenTabs = (0, _tabs.getHiddenTabs)(tabSources, sourceTabEls);

      if ((0, _ui.isVisible)() && hiddenTabs.indexOf(selectedSource) !== -1) {
        return moveTab(selectedSource.url, 0);
      }

      this.setState({
        hiddenTabs
      });
    };

    this.renderDropdownSource = source => {
      const {
        selectSpecificSource
      } = this.props;
      const filename = (0, _source.getFilename)(source.toJS());

      const onClick = () => selectSpecificSource(source.id);

      return _react2.default.createElement("li", {
        key: source.id,
        onClick: onClick
      }, _react2.default.createElement("img", {
        className: `dropdown-icon ${this.getIconClass(source)}`
      }), filename);
    };

    this.state = {
      dropdownShown: false,
      hiddenTabs: I.List()
    };
    this.onResize = (0, _lodash.debounce)(() => {
      this.updateHiddenTabs();
    });
  }

  componentDidUpdate(prevProps) {
    if (!(prevProps === this.props)) {
      this.updateHiddenTabs();
    }
  }

  componentDidMount() {
    window.requestIdleCallback(this.updateHiddenTabs);
    window.addEventListener("resize", this.onResize);
  }

  componentWillUnmount() {
    window.removeEventListener("resize", this.onResize);
  }
  /*
   * Updates the hiddenSourceTabs state, by
   * finding the source tabs which are wrapped and are not on the top row.
   */


  toggleSourcesDropdown(e) {
    this.setState(prevState => ({
      dropdownShown: !prevState.dropdownShown
    }));
  }

  getIconClass(source) {
    if ((0, _source.isPretty)(source)) {
      return "prettyPrint";
    }

    if (source.isBlackBoxed) {
      return "blackBox";
    }

    return "file";
  }

  renderTabs() {
    const {
      tabSources
    } = this.props;

    if (!tabSources) {
      return;
    }

    return _react2.default.createElement("div", {
      className: "source-tabs",
      ref: "sourceTabs"
    }, tabSources.map((source, index) => _react2.default.createElement(_Tab2.default, {
      key: index,
      source: source
    })));
  }

  renderDropdown() {
    const hiddenTabs = this.state.hiddenTabs;

    if (!hiddenTabs || hiddenTabs.size == 0) {
      return null;
    }

    const Panel = _react2.default.createElement("ul", null, hiddenTabs.map(this.renderDropdownSource));

    const icon = _react2.default.createElement("img", {
      className: "moreTabs"
    });

    return _react2.default.createElement(_Dropdown2.default, {
      panel: Panel,
      icon: icon
    });
  }

  renderStartPanelToggleButton() {
    return _react2.default.createElement(_PaneToggle2.default, {
      position: "start",
      collapsed: !this.props.startPanelCollapsed,
      handleClick: this.props.togglePaneCollapse
    });
  }

  renderEndPanelToggleButton() {
    const {
      horizontal,
      endPanelCollapsed,
      togglePaneCollapse
    } = this.props;

    if (!horizontal) {
      return;
    }

    return _react2.default.createElement(_PaneToggle2.default, {
      position: "end",
      collapsed: !endPanelCollapsed,
      handleClick: togglePaneCollapse,
      horizontal: horizontal
    });
  }

  render() {
    return _react2.default.createElement("div", {
      className: "source-header"
    }, this.renderStartPanelToggleButton(), this.renderTabs(), this.renderDropdown(), this.renderEndPanelToggleButton());
  }

}

const mapStateToProps = state => ({
  selectedSource: (0, _selectors.getSelectedSource)(state),
  tabSources: (0, _selectors.getSourcesForTabs)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Tabs);