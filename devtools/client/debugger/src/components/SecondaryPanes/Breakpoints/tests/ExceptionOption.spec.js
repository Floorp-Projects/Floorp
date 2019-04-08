/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import ExceptionOption from "../ExceptionOption";

describe("ExceptionOption renders", () => {
  it("with values", () => {
    const component = shallow(
      <ExceptionOption
        label="testLabel"
        isChecked={true}
        onChange={() => null}
        className="testClassName"
      />
    );
    expect(component).toMatchSnapshot();
  });
});
