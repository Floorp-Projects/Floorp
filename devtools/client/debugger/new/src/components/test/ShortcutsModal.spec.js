"use strict";

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _enzyme = require("enzyme/index");

var _ShortcutsModal = require("../ShortcutsModal");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
describe("ShortcutsModal", () => {
  it("renders when enabled", () => {
    const enabled = true;
    const wrapper = (0, _enzyme.shallow)(_react2.default.createElement(_ShortcutsModal.ShortcutsModal, {
      enabled: enabled
    }));
    expect(wrapper).toMatchSnapshot();
  });
  it("renders nothing when not enabled", () => {
    const wrapper = (0, _enzyme.shallow)(_react2.default.createElement(_ShortcutsModal.ShortcutsModal, null));
    expect(wrapper.text()).toBe("");
  });
});