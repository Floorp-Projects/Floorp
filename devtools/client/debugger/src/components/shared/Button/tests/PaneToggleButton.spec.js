/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";
import { PaneToggleButton } from "../";

describe("PaneToggleButton", () => {
  const handleClickSpy = jest.fn();
  const wrapper = shallow(
    React.createElement(PaneToggleButton, {
      handleClick: handleClickSpy,
      collapsed: false,
      position: "start",
    })
  );

  it("renders default", () => {
    expect(wrapper.hasClass("vertical")).toBe(true);
    expect(wrapper).toMatchSnapshot();
  });

  it("toggles horizontal class", () => {
    wrapper.setProps({ horizontal: true });
    expect(wrapper.hasClass("vertical")).toBe(false);
  });

  it("toggles collapsed class", () => {
    wrapper.setProps({ collapsed: true });
    expect(wrapper.hasClass("collapsed")).toBe(true);
  });

  it("toggles start position", () => {
    wrapper.setProps({ position: "start" });
    expect(wrapper.hasClass("start")).toBe(true);
  });

  it("toggles end position", () => {
    wrapper.setProps({ position: "end" });
    expect(wrapper.hasClass("end")).toBe(true);
  });

  it("handleClick is called", () => {
    const position = "end";
    const collapsed = false;
    wrapper.setProps({ position, collapsed });
    wrapper.simulate("click");
    expect(handleClickSpy).toHaveBeenCalledWith(position, true);
  });
});
