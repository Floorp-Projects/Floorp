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

var _devtoolsReps = require("devtools/client/shared/components/reps/reps.js");

var _preview = require("../../utils/preview");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  createNode,
  getChildren
} = _devtoolsReps.ObjectInspectorUtils.node;
const {
  loadItemProperties
} = _devtoolsReps.ObjectInspectorUtils.loadProperties;

class FrameworkComponent extends _react.PureComponent {
  async componentWillMount() {
    const expression = "this;";
    const {
      selectedFrame,
      setPopupObjectProperties
    } = this.props;
    const value = selectedFrame.this;
    const root = createNode({
      name: expression,
      contents: {
        value
      }
    });
    const properties = await loadItemProperties(root, _firefox.createObjectClient);

    if (properties) {
      setPopupObjectProperties(value, properties);
    }
  }

  renderReactComponent() {
    const {
      selectedFrame,
      popupObjectProperties
    } = this.props;
    const expression = "this;";
    const value = selectedFrame.this;
    const root = {
      name: expression,
      path: expression,
      contents: {
        value
      }
    };
    const loadedRootProperties = popupObjectProperties[value.actor];

    if (!loadedRootProperties) {
      return null;
    }

    let roots = getChildren({
      item: root,
      loadedProperties: new Map([[root.path, loadedRootProperties]])
    });
    roots = roots.filter(r => ["state", "props"].includes(r.name));
    return _react2.default.createElement("div", {
      className: "pane framework-component"
    }, _react2.default.createElement(_devtoolsReps.ObjectInspector, {
      roots: roots,
      autoExpandAll: false,
      autoExpandDepth: 0,
      disableWrap: true,
      focusable: false,
      dimTopLevelWindow: true,
      createObjectClient: grip => (0, _firefox.createObjectClient)(grip)
    }));
  }

  render() {
    const {
      selectedFrame
    } = this.props;

    if (selectedFrame && (0, _preview.isReactComponent)(selectedFrame.this)) {
      return this.renderReactComponent();
    }

    return null;
  }

}

const mapStateToProps = state => ({
  selectedFrame: (0, _selectors.getSelectedFrame)(state),
  popupObjectProperties: (0, _selectors.getAllPopupObjectProperties)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(FrameworkComponent);