"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _prefs = require("../../utils/prefs");

var _source = require("../../utils/source");

var _sources = require("../../reducers/sources");

var _editor = require("../../utils/editor/index");

var _Button = require("../shared/Button/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class SourceFooter extends _react.PureComponent {
  prettyPrintButton() {
    const {
      selectedSource,
      togglePrettyPrint
    } = this.props;
    const sourceLoaded = selectedSource && (0, _source.isLoaded)(selectedSource);

    if (!(0, _editor.shouldShowPrettyPrint)(selectedSource)) {
      return;
    }

    const tooltip = L10N.getStr("sourceTabs.prettyPrint");
    const type = "prettyPrint";
    return _react2.default.createElement("button", {
      onClick: () => togglePrettyPrint(selectedSource.id),
      className: (0, _classnames2.default)("action", type, {
        active: sourceLoaded,
        pretty: (0, _source.isPretty)(selectedSource)
      }),
      key: type,
      title: tooltip,
      "aria-label": tooltip
    }, _react2.default.createElement("img", {
      className: type
    }));
  }

  blackBoxButton() {
    const {
      selectedSource,
      toggleBlackBox
    } = this.props;
    const sourceLoaded = selectedSource && (0, _source.isLoaded)(selectedSource);

    if (!sourceLoaded || selectedSource.isPrettyPrinted) {
      return;
    }

    const blackboxed = selectedSource.isBlackBoxed;
    const tooltip = L10N.getStr("sourceFooter.blackbox");
    const type = "black-box";
    return _react2.default.createElement("button", {
      onClick: () => toggleBlackBox(selectedSource),
      className: (0, _classnames2.default)("action", type, {
        active: sourceLoaded,
        blackboxed: blackboxed
      }),
      key: type,
      title: tooltip,
      "aria-label": tooltip
    }, _react2.default.createElement("img", {
      className: "blackBox"
    }));
  }

  blackBoxSummary() {
    const {
      selectedSource
    } = this.props;

    if (!selectedSource || !selectedSource.isBlackBoxed) {
      return;
    }

    return _react2.default.createElement("span", {
      className: "blackbox-summary"
    }, L10N.getStr("sourceFooter.blackboxed"));
  }

  coverageButton() {
    const {
      recordCoverage
    } = this.props;

    if (!_prefs.features.codeCoverage) {
      return;
    }

    return _react2.default.createElement("button", {
      className: "coverage action",
      title: L10N.getStr("sourceFooter.codeCoverage"),
      onClick: () => recordCoverage(),
      "aria-label": L10N.getStr("sourceFooter.codeCoverage")
    }, "C");
  }

  renderToggleButton() {
    if (this.props.horizontal) {
      return;
    }

    return _react2.default.createElement(_Button.PaneToggleButton, {
      position: "end",
      collapsed: !this.props.endPanelCollapsed,
      horizontal: this.props.horizontal,
      handleClick: this.props.togglePaneCollapse
    });
  }

  renderCommands() {
    return _react2.default.createElement("div", {
      className: "commands"
    }, this.prettyPrintButton(), this.blackBoxButton(), this.blackBoxSummary(), this.coverageButton());
  }

  renderSourceSummary() {
    const {
      mappedSource,
      jumpToMappedLocation,
      selectedSource
    } = this.props;

    if (!mappedSource) {
      return null;
    }

    const filename = (0, _source.getFilename)(mappedSource);
    const tooltip = L10N.getFormatStr("sourceFooter.mappedSourceTooltip", filename);
    const title = L10N.getFormatStr("sourceFooter.mappedSource", filename);
    const mappedSourceLocation = {
      sourceId: selectedSource.id,
      line: 1,
      column: 1
    };
    return _react2.default.createElement("button", {
      className: "mapped-source",
      onClick: () => jumpToMappedLocation(mappedSourceLocation),
      title: tooltip
    }, _react2.default.createElement("span", null, title));
  }

  render() {
    const {
      selectedSource,
      horizontal
    } = this.props;

    if (!(0, _editor.shouldShowFooter)(selectedSource, horizontal)) {
      return null;
    }

    return _react2.default.createElement("div", {
      className: "source-footer"
    }, this.renderCommands(), this.renderSourceSummary(), this.renderToggleButton());
  }

}

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const selectedId = selectedSource.id;
  return {
    selectedSource,
    mappedSource: (0, _sources.getGeneratedSource)(state, selectedSource),
    prettySource: (0, _selectors.getPrettySource)(state, selectedId),
    endPanelCollapsed: (0, _selectors.getPaneCollapse)(state, "end")
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  togglePrettyPrint: _actions2.default.togglePrettyPrint,
  toggleBlackBox: _actions2.default.toggleBlackBox,
  jumpToMappedLocation: _actions2.default.jumpToMappedLocation,
  recordCoverage: _actions2.default.recordCoverage,
  togglePaneCollapse: _actions2.default.togglePaneCollapse
})(SourceFooter);