/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import ResultList from "../ResultList";

const selectItem = jest.fn();
const selectedIndex = 1;
const payload = {
  items: [
    {
      id: 0,
      subtitle: "subtitle",
      title: "title",
      value: "value",
    },
    {
      id: 1,
      subtitle: "subtitle 1",
      title: "title 1",
      value: "value 1",
    },
  ],
  selected: selectedIndex,
  selectItem,
};

describe("Result list", () => {
  it("should call onClick function", () => {
    const wrapper = shallow(<ResultList {...payload} />);

    wrapper.childAt(selectedIndex).simulate("click");
    expect(selectItem).toHaveBeenCalled();
  });

  it("should render the component", () => {
    const wrapper = shallow(<ResultList {...payload} />);
    expect(wrapper).toMatchSnapshot();
  });

  it("selected index should have 'selected class'", () => {
    const wrapper = shallow(<ResultList {...payload} />);
    const childHasClass = wrapper.childAt(selectedIndex).hasClass("selected");

    expect(childHasClass).toEqual(true);
  });
});
