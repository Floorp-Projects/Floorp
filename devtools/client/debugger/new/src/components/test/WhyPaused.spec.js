"use strict";

var _enzyme = require("enzyme/index");

var _WhyPaused = require("../SecondaryPanes/Frames/WhyPaused.js/index");

var _WhyPaused2 = _interopRequireDefault(_WhyPaused);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
describe("WhyPaused", () => {
  it("should pause reason with message", () => {
    const why = {
      type: "breakpoint",
      message: "bla is hit"
    };
    const component = (0, _enzyme.shallow)((0, _WhyPaused2.default)(why));
    expect(component).toMatchSnapshot();
  });
  it("should show pause reason with exception details", () => {
    const why = {
      type: "exception",
      exception: {
        class: "Error",
        preview: {
          name: "ReferenceError",
          message: "o is not defined"
        }
      }
    };
    const component = (0, _enzyme.shallow)((0, _WhyPaused2.default)(why));
    expect(component).toMatchSnapshot();
  });
  it("should show pause reason with exception string", () => {
    const why = {
      type: "exception",
      exception: "Not Available"
    };
    const component = (0, _enzyme.shallow)((0, _WhyPaused2.default)(why));
    expect(component).toMatchSnapshot();
  });
});