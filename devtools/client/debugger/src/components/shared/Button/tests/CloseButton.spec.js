/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";
import { CloseButton } from "../";

describe("CloseButton", () => {
  it("renders with tooltip", () => {
    const tooltip = "testTooltip";
    const wrapper = shallow(
      React.createElement(CloseButton, {
        tooltip: tooltip,
        handleClick: () => {},
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("handles click event", () => {
    const handleClickSpy = jest.fn();
    const wrapper = shallow(
      React.createElement(CloseButton, {
        handleClick: handleClickSpy,
      })
    );
    wrapper.simulate("click");
    expect(handleClickSpy).toHaveBeenCalled();
  });
});
