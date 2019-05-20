/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import { CommandBarButton, debugBtn } from "../";

describe("CommandBarButton", () => {
  it("renders", () => {
    const wrapper = shallow(
      <CommandBarButton children={([]: any)} className={""} />
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders children", () => {
    const children = [1, 2, 3, 4];
    const wrapper = shallow(
      <CommandBarButton children={(children: any)} className={""} />
    );
    expect(wrapper.find("button").children()).toHaveLength(4);
  });
});

describe("debugBtn", () => {
  it("renders", () => {
    const wrapper = shallow(<debugBtn />);
    expect(wrapper).toMatchSnapshot();
  });

  it("handles onClick", () => {
    const onClickSpy = jest.fn();
    const wrapper = shallow(<debugBtn onClick={onClickSpy} />);
    wrapper.simulate("click");
    expect(onClickSpy).toHaveBeenCalled();
  });
});
