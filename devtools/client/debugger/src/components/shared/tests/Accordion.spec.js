/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";

import Accordion from "../Accordion";

describe("Accordion", () => {
  const testItems = [
    {
      header: "Test Accordion Item 1",
      id: "accordion-item-1",
      className: "accordion-item-1",
      component: React.createElement("div", null),
      opened: false,
      onToggle: jest.fn(),
    },
    {
      header: "Test Accordion Item 2",
      id: "accordion-item-2",
      className: "accordion-item-2",
      component: React.createElement("div", null),
      buttons: React.createElement("button", null),
      opened: false,
      onToggle: jest.fn(),
    },
    {
      header: "Test Accordion Item 3",
      id: "accordion-item-3",
      className: "accordion-item-3",
      component: React.createElement("div", null),
      opened: true,
      onToggle: jest.fn(),
    },
  ];
  const wrapper = shallow(
    React.createElement(Accordion, {
      items: testItems,
    })
  );
  it("basic render", () => expect(wrapper).toMatchSnapshot());
  wrapper.find(".accordion-item-1 button").simulate("click");
  it("handleClick and onToggle", () =>
    expect(testItems[0].onToggle).toHaveBeenCalledWith(true));
});
