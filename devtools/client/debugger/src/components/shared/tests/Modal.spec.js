/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import { Modal } from "../Modal";

describe("Modal", () => {
  it("renders", () => {
    const wrapper = shallow(<Modal handleClose={() => {}} status="entering" />);
    expect(wrapper).toMatchSnapshot();
  });

  it("handles close modal click", () => {
    const handleCloseSpy = jest.fn();
    const wrapper = shallow(
      <Modal handleClose={handleCloseSpy} status="entering" />
    );
    wrapper.find(".modal-wrapper").simulate("click");
    expect(handleCloseSpy).toHaveBeenCalled();
  });

  it("renders children", () => {
    const children = <div className="aChild" />;
    const wrapper = shallow(
      <Modal children={children} handleClose={() => {}} status="entering" />
    );
    expect(wrapper.find(".aChild")).toHaveLength(1);
  });

  it("passes additionalClass to child div class", () => {
    const additionalClass = "testAddon";
    const wrapper = shallow(
      <Modal
        additionalClass={additionalClass}
        handleClose={() => {}}
        status="entering"
      />
    );
    expect(wrapper.find(`.modal-wrapper .${additionalClass}`)).toHaveLength(1);
  });

  it("passes status to child div class", () => {
    const status: any = "testStatus";
    const wrapper = shallow(<Modal status={status} handleClose={() => {}} />);
    expect(wrapper.find(`.modal-wrapper .${status}`)).toHaveLength(1);
  });
});
