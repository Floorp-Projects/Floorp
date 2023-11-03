/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import Modal from "../Modal";

describe("Modal", () => {
  it("renders", () => {
    const wrapper = shallow(
      React.createElement(Modal, {
        handleClose: () => {},
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("handles close modal click", () => {
    const handleCloseSpy = jest.fn();
    const wrapper = shallow(
      React.createElement(Modal, {
        handleClose: handleCloseSpy,
      })
    );
    wrapper.find(".modal-wrapper").simulate("click");
    expect(handleCloseSpy).toHaveBeenCalled();
  });

  it("renders children", () => {
    const wrapper = shallow(
      React.createElement(
        Modal,
        {
          handleClose: () => {},
        },
        React.createElement("div", {
          className: "aChild",
        })
      )
    );
    expect(wrapper.find(".aChild")).toHaveLength(1);
  });

  it("passes additionalClass to child div class", () => {
    const additionalClass = "testAddon";
    const wrapper = shallow(
      React.createElement(Modal, {
        additionalClass,
        handleClose: () => {},
      })
    );
    expect(wrapper.find(`.modal-wrapper .${additionalClass}`)).toHaveLength(1);
  });
});
