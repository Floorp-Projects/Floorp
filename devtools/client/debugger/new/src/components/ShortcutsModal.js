"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ShortcutsModal = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _Modal = require("./shared/Modal");

var _Modal2 = _interopRequireDefault(_Modal);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _text = require("../utils/text");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class ShortcutsModal extends _react.Component {
  renderPrettyCombos(combo) {
    return combo.split(" ").map(c => _react2.default.createElement("span", {
      key: c,
      className: "keystroke"
    }, c)).reduce((prev, curr) => [prev, " + ", curr]);
  }

  renderShorcutItem(title, combo) {
    return _react2.default.createElement("li", null, _react2.default.createElement("span", null, title), _react2.default.createElement("span", null, this.renderPrettyCombos(combo)));
  }

  renderEditorShortcuts() {
    return _react2.default.createElement("ul", {
      className: "shortcuts-list"
    }, this.renderShorcutItem(L10N.getStr("shortcuts.toggleBreakpoint"), (0, _text.formatKeyShortcut)(L10N.getStr("toggleBreakpoint.key"))), this.renderShorcutItem(L10N.getStr("shortcuts.toggleCondPanel"), (0, _text.formatKeyShortcut)(L10N.getStr("toggleCondPanel.key"))));
  }

  renderSteppingShortcuts() {
    return _react2.default.createElement("ul", {
      className: "shortcuts-list"
    }, this.renderShorcutItem(L10N.getStr("shortcuts.pauseOrResume"), "F8"), this.renderShorcutItem(L10N.getStr("shortcuts.stepOver"), "F10"), this.renderShorcutItem(L10N.getStr("shortcuts.stepIn"), "F11"), this.renderShorcutItem(L10N.getStr("shortcuts.stepOut"), (0, _text.formatKeyShortcut)(L10N.getStr("stepOut.key"))));
  }

  renderSearchShortcuts() {
    return _react2.default.createElement("ul", {
      className: "shortcuts-list"
    }, this.renderShorcutItem(L10N.getStr("shortcuts.fileSearch"), (0, _text.formatKeyShortcut)(L10N.getStr("sources.search.key2"))), this.renderShorcutItem(L10N.getStr("shortcuts.searchAgain"), (0, _text.formatKeyShortcut)(L10N.getStr("sourceSearch.search.again.key2"))), this.renderShorcutItem(L10N.getStr("shortcuts.projectSearch"), (0, _text.formatKeyShortcut)(L10N.getStr("projectTextSearch.key"))), this.renderShorcutItem(L10N.getStr("shortcuts.functionSearch"), (0, _text.formatKeyShortcut)(L10N.getStr("functionSearch.key"))), this.renderShorcutItem(L10N.getStr("shortcuts.gotoLine"), (0, _text.formatKeyShortcut)(L10N.getStr("gotoLineModal.key2"))));
  }

  renderShortcutsContent() {
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("shortcuts-content", this.props.additionalClass)
    }, _react2.default.createElement("div", {
      className: "shortcuts-section"
    }, _react2.default.createElement("h2", null, L10N.getStr("shortcuts.header.editor")), this.renderEditorShortcuts()), _react2.default.createElement("div", {
      className: "shortcuts-section"
    }, _react2.default.createElement("h2", null, L10N.getStr("shortcuts.header.stepping")), this.renderSteppingShortcuts()), _react2.default.createElement("div", {
      className: "shortcuts-section"
    }, _react2.default.createElement("h2", null, L10N.getStr("shortcuts.header.search")), this.renderSearchShortcuts()));
  }

  render() {
    const {
      enabled
    } = this.props;

    if (!enabled) {
      return null;
    }

    return _react2.default.createElement(_Modal2.default, {
      "in": enabled,
      additionalClass: "shortcuts-modal",
      handleClose: this.props.handleClose
    }, this.renderShortcutsContent());
  }

}

exports.ShortcutsModal = ShortcutsModal;