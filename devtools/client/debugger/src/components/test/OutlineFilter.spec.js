/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import OutlineFilter from "../../components/PrimaryPanes/OutlineFilter";

function generateDefaults(overrides) {
  return {
    filter: "",
    updateFilter: jest.fn(),
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(<OutlineFilter {...props} />);
  const instance = component.instance();
  return { component, props, instance };
}

describe("OutlineFilter", () => {
  it("shows an input with no value when filter is empty", async () => {
    const { component } = render({ filter: "" });
    expect(component).toMatchSnapshot();
  });

  it("shows an input with the filter when it is not empty", async () => {
    const { component } = render({ filter: "abc" });
    expect(component).toMatchSnapshot();
  });

  it("calls props.updateFilter on change", async () => {
    const updateFilter = jest.fn();
    const { component } = render({ updateFilter });
    const input = component.find("input");
    input.simulate("change", { target: { value: "a" } });
    input.simulate("change", { target: { value: "ab" } });
    expect(updateFilter).toHaveBeenCalled();
    expect(updateFilter.mock.calls[0][0]).toBe("a");
    expect(updateFilter.mock.calls[1][0]).toBe("ab");
  });
});
