/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import { Modal } from "../Modal";

describe("Modal", () => {
  it("renders", () => {
    const wrapper = shallow(<Modal />);
    expect(wrapper).toMatchSnapshot();
  });

  it("handles close modal click", () => {
    const handleCloseSpy = jest.fn();
    const wrapper = shallow(<Modal handleClose={handleCloseSpy} />);
    wrapper.find(".modal-wrapper").simulate("click");
    expect(handleCloseSpy).toBeCalled();
  });

  it("renders children", () => {
    const children = <div className="aChild" />;
    const wrapper = shallow(<Modal children={children} />);
    expect(wrapper.find(".aChild")).toHaveLength(1);
  });

  it("passes additionalClass to child div class", () => {
    const additionalClass = "testAddon";
    const wrapper = shallow(<Modal additionalClass={additionalClass} />);
    expect(wrapper.find(`.modal-wrapper .${additionalClass}`)).toHaveLength(1);
  });

  it("passes status to child div class", () => {
    const status = "testStatus";
    const wrapper = shallow(<Modal status={status} />);
    expect(wrapper.find(`.modal-wrapper .${status}`)).toHaveLength(1);
  });
});
