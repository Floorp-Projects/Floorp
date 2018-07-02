"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _text = require("../../utils/text");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _prefs = require("../../utils/prefs");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _Outline = require("./Outline");

var _Outline2 = _interopRequireDefault(_Outline);

var _SourcesTree = require("./SourcesTree");

var _SourcesTree2 = _interopRequireDefault(_SourcesTree);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class PrimaryPanes extends _react.Component {
  constructor(props) {
    super(props);

    this.showPane = selectedPane => {
      this.props.setPrimaryPaneTab(selectedPane);
    };

    this.onAlphabetizeClick = () => {
      const alphabetizeOutline = !_prefs.prefs.alphabetizeOutline;
      _prefs.prefs.alphabetizeOutline = alphabetizeOutline;
      this.setState({
        alphabetizeOutline
      });
    };

    this.renderTabs = () => {
      return _react2.default.createElement("div", {
        className: "source-outline-tabs"
      }, this.renderOutlineTabs());
    };

    this.renderShortcut = () => {
      if (this.props.horizontal) {
        const onClick = () => {
          if (this.props.sourceSearchOn) {
            return this.props.closeActiveSearch();
          }

          this.props.setActiveSearch("source");
        };

        return _react2.default.createElement("span", {
          className: "sources-header-info",
          dir: "ltr",
          onClick: onClick
        }, L10N.getFormatStr("sources.search", (0, _text.formatKeyShortcut)(L10N.getStr("sources.search.key2"))));
      }
    };

    this.state = {
      alphabetizeOutline: _prefs.prefs.alphabetizeOutline
    };
  }

  renderOutlineTabs() {
    if (!_prefs.features.outline) {
      return;
    }

    const sources = (0, _text.formatKeyShortcut)(L10N.getStr("sources.header"));
    const outline = (0, _text.formatKeyShortcut)(L10N.getStr("outline.header"));
    return [_react2.default.createElement("div", {
      className: (0, _classnames2.default)("tab sources-tab", {
        active: this.props.selectedTab === "sources"
      }),
      onClick: () => this.showPane("sources"),
      key: "sources-tab"
    }, sources), _react2.default.createElement("div", {
      className: (0, _classnames2.default)("tab outline-tab", {
        active: this.props.selectedTab === "outline"
      }),
      onClick: () => this.showPane("outline"),
      key: "outline-tab"
    }, outline)];
  }

  render() {
    const {
      selectedTab
    } = this.props;
    return _react2.default.createElement("div", {
      className: "sources-panel"
    }, this.renderTabs(), selectedTab === "sources" ? _react2.default.createElement(_SourcesTree2.default, null) : _react2.default.createElement(_Outline2.default, {
      alphabetizeOutline: this.state.alphabetizeOutline,
      onAlphabetizeClick: this.onAlphabetizeClick
    }));
  }

}

const mapStateToProps = state => ({
  selectedTab: (0, _selectors.getSelectedPrimaryPaneTab)(state),
  sources: (0, _selectors.getSources)(state),
  sourceSearchOn: (0, _selectors.getActiveSearch)(state) === "source"
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  setPrimaryPaneTab: _actions2.default.setPrimaryPaneTab,
  selectLocation: _actions2.default.selectLocation,
  setActiveSearch: _actions2.default.setActiveSearch,
  closeActiveSearch: _actions2.default.closeActiveSearch
})(PrimaryPanes);