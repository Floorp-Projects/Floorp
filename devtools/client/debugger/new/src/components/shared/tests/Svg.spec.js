/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { shallow } from "enzyme";
import Svg from "../Svg";

describe("Svg", () => {
  it("renders with props", () => {
    const wrapper = shallow(
      Svg({
        name: "webpack",
        className: "class",
        onClick: () => {},
        "aria-label": "label"
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("render nothing if invalid svg name", () => {
    const svg = Svg({ name: "reallyRandomNameHere" });
    expect(svg).toBe(null);
  });

  it("calls onClick method on click", () => {
    const onClick = jest.fn();
    const wrapper = shallow(
      Svg({
        name: "webpack",
        className: "class",
        onClick,
        "aria-label": "label"
      })
    );
    wrapper.simulate("click");
    expect(onClick).toBeCalledTimes(1);
  });
});
