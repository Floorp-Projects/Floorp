/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";
import WhyPaused from "../SecondaryPanes/WhyPaused.js";

function render(why, delay) {
  const props = { why, delay };
  const component = shallow(
    React.createElement(WhyPaused.WrappedComponent, props)
  );

  return { component, props };
}

describe("WhyPaused", () => {
  it("should pause reason with message", () => {
    const why = {
      type: "breakpoint",
      message: "bla is hit",
    };
    const { component } = render(why);
    expect(component).toMatchSnapshot();
  });

  it("should show pause reason with exception details", () => {
    const why = {
      type: "exception",
      exception: {
        class: "ReferenceError",
        isError: true,
        preview: {
          name: "ReferenceError",
          message: "o is not defined",
        },
      },
    };

    const { component } = render(why);
    expect(component).toMatchSnapshot();
  });

  it("should show pause reason with exception string", () => {
    const why = {
      type: "exception",
      exception: "Not Available",
    };

    const { component } = render(why);
    expect(component).toMatchSnapshot();
  });

  it("should show an empty div when there is no pause reason", () => {
    const why = undefined;

    const { component } = render(why);
    expect(component).toMatchSnapshot();
  });
});
