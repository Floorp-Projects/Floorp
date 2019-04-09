/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import Dropdown from "../Dropdown";

describe("Dropdown", () => {
  const wrapper = shallow(<Dropdown panel={<div />} icon="✅" />);
  it("render", () => expect(wrapper).toMatchSnapshot());
  wrapper.find(".dropdown").simulate("click");
  it("handle toggleDropdown", () =>
    expect(wrapper.state().dropdownShown).toEqual(true));
});
