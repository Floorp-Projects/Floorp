/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import Accordion from "../Accordion";

describe("Accordion", () => {
  const testItems = [
    {
      header: "Test Accordion Item 1",
      className: "accordion-item-1",
      component: <div />,
      opened: false,
      onToggle: jest.fn(),
    },
    {
      header: "Test Accordion Item 2",
      className: "accordion-item-2",
      component: <div />,
      buttons: <button />,
      opened: false,
      onToggle: jest.fn(),
    },
    {
      header: "Test Accordion Item 3",
      className: "accordion-item-3",
      component: <div />,
      opened: true,
      onToggle: jest.fn(),
    },
  ];
  const wrapper = shallow(<Accordion items={testItems} />);
  it("basic render", () => expect(wrapper).toMatchSnapshot());
  wrapper.find(".accordion-item-1 ._header").simulate("click");
  it("handleClick and onToggle", () =>
    expect(testItems[0].onToggle).toHaveBeenCalledWith(true));
});
