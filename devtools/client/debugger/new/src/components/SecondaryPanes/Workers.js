"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Workers = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _path = require("../../utils/path");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Workers extends _react.PureComponent {
  renderWorkers(workers) {
    const {
      openWorkerToolbox
    } = this.props;
    return workers.map(worker => _react2.default.createElement("div", {
      className: "worker",
      key: worker.actor,
      onClick: () => openWorkerToolbox(worker)
    }, _react2.default.createElement("img", {
      className: "domain"
    }), (0, _path.basename)(worker.url)));
  }

  renderNoWorkersPlaceholder() {
    return _react2.default.createElement("div", {
      className: "pane-info"
    }, L10N.getStr("noWorkersText"));
  }

  render() {
    const {
      workers
    } = this.props;
    return _react2.default.createElement("div", {
      className: "pane workers-list"
    }, workers && workers.size > 0 ? this.renderWorkers(workers) : this.renderNoWorkersPlaceholder());
  }

}

exports.Workers = Workers;

const mapStateToProps = state => ({
  workers: (0, _selectors.getWorkers)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Workers);