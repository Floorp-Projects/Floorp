/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import { ShortcutsModal } from "../ShortcutsModal";

function render(overrides = {}) {
  const props = {
    enabled: true,
    handleClose: jest.fn(),
    additionalClass: "",
    ...overrides,
  };
  const component = shallow(<ShortcutsModal {...props} />);

  return { component, props };
}

describe("ShortcutsModal", () => {
  it("renders when enabled", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("renders nothing when not enabled", () => {
    const { component } = render({
      enabled: false,
    });
    expect(component.text()).toBe("");
  });

  it("renders with additional classname", () => {
    const { component } = render({
      additionalClass: "additional-class",
    });
    expect(
      component.find(".shortcuts-content").hasClass("additional-class")
    ).toEqual(true);
  });
});
