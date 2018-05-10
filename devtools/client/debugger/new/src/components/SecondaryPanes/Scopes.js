"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _firefox = require("../../client/firefox");

var _selectors = require("../../selectors/index");

var _scopes = require("../../utils/pause/scopes/index");

var _devtoolsReps = require("devtools/client/shared/components/reps/reps.js");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Scopes extends _react.PureComponent {
  constructor(props, ...args) {
    const {
      why,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes
    } = props;
    super(props, ...args);
    this.state = {
      originalScopes: (0, _scopes.getScopes)(why, selectedFrame, originalFrameScopes),
      generatedScopes: (0, _scopes.getScopes)(why, selectedFrame, generatedFrameScopes),
      showOriginal: true
    };
  }

  componentWillReceiveProps(nextProps) {
    const {
      isPaused,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes
    } = this.props;
    const isPausedChanged = isPaused !== nextProps.isPaused;
    const selectedFrameChanged = selectedFrame !== nextProps.selectedFrame;
    const originalFrameScopesChanged = originalFrameScopes !== nextProps.originalFrameScopes;
    const generatedFrameScopesChanged = generatedFrameScopes !== nextProps.generatedFrameScopes;

    if (isPausedChanged || selectedFrameChanged || originalFrameScopesChanged || generatedFrameScopesChanged) {
      this.setState({
        originalScopes: (0, _scopes.getScopes)(nextProps.why, nextProps.selectedFrame, nextProps.originalFrameScopes),
        generatedScopes: (0, _scopes.getScopes)(nextProps.why, nextProps.selectedFrame, nextProps.generatedFrameScopes)
      });
    }
  }

  render() {
    const {
      isPaused,
      isLoading
    } = this.props;
    const {
      originalScopes,
      generatedScopes,
      showOriginal
    } = this.state;
    const scopes = showOriginal && originalScopes || generatedScopes;

    if (scopes && !isLoading) {
      return _react2.default.createElement("div", {
        className: "pane scopes-list"
      }, _react2.default.createElement(_devtoolsReps.ObjectInspector, {
        roots: scopes,
        autoExpandAll: false,
        autoExpandDepth: 1,
        disableWrap: true,
        focusable: false,
        dimTopLevelWindow: true,
        createObjectClient: grip => (0, _firefox.createObjectClient)(grip)
      }), originalScopes ? _react2.default.createElement("div", {
        className: "scope-type-toggle"
      }, _react2.default.createElement("a", {
        href: "",
        onClick: e => {
          e.preventDefault();
          this.setState({
            showOriginal: !showOriginal
          });
        }
      }, showOriginal ? L10N.getStr("scopes.toggleToGenerated") : L10N.getStr("scopes.toggleToOriginal"))) : null);
    }

    let stateText = L10N.getStr("scopes.notPaused");

    if (isPaused) {
      if (isLoading) {
        stateText = L10N.getStr("loadingText");
      } else {
        stateText = L10N.getStr("scopes.notAvailable");
      }
    }

    return _react2.default.createElement("div", {
      className: "pane scopes-list"
    }, _react2.default.createElement("div", {
      className: "pane-info"
    }, stateText));
  }

}

const mapStateToProps = state => {
  const selectedFrame = (0, _selectors.getSelectedFrame)(state);
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const {
    scope: originalFrameScopes,
    pending: originalPending
  } = (0, _selectors.getOriginalFrameScope)(state, selectedSource && selectedSource.get("id"), selectedFrame && selectedFrame.id) || {
    scope: null,
    pending: false
  };
  const {
    scope: generatedFrameScopes,
    pending: generatedPending
  } = (0, _selectors.getGeneratedFrameScope)(state, selectedFrame && selectedFrame.id) || {
    scope: null,
    pending: false
  };
  return {
    selectedFrame,
    isPaused: (0, _selectors.isPaused)(state),
    isLoading: generatedPending || originalPending,
    why: (0, _selectors.getPauseReason)(state),
    originalFrameScopes,
    generatedFrameScopes
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Scopes);