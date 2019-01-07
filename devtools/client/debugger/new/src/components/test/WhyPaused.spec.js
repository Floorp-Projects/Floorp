/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { shallow } from "enzyme";
import renderWhyPaused from "../SecondaryPanes/Frames/WhyPaused.js";

describe("WhyPaused", () => {
  it("should pause reason with message", () => {
    const why = {
      type: "breakpoint",
      message: "bla is hit"
    };
    const component = shallow(renderWhyPaused(why));
    expect(component).toMatchSnapshot();
  });

  it("should show pause reason with exception details", () => {
    const why = {
      type: "exception",
      exception: {
        class: "Error",
        preview: {
          name: "ReferenceError",
          message: "o is not defined"
        }
      }
    };

    const component = shallow(renderWhyPaused(why));
    expect(component).toMatchSnapshot();
  });

  it("should show pause reason with exception string", () => {
    const why = {
      type: "exception",
      exception: "Not Available"
    };

    const component = shallow(renderWhyPaused(why));
    expect(component).toMatchSnapshot();
  });
});
