/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";

import BracketArrow from "../BracketArrow";

describe("BracketArrow", () => {
  const wrapper = shallow(
    React.createElement(BracketArrow, {
      orientation: "down",
      left: 10,
      top: 20,
      bottom: 50,
    })
  );
  it("render", () => expect(wrapper).toMatchSnapshot());
  it("render up", () => {
    wrapper.setProps({ orientation: null });
    expect(wrapper).toMatchSnapshot();
  });
});
