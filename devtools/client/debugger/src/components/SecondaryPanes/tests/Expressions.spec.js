/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";
import Expressions from "../Expressions";

function generateDefaults(overrides) {
  return {
    evaluateExpressions: async () => {},
    expressions: [
      {
        input: "expression1",
        value: {
          result: {
            value: "foo",
            class: "",
          },
        },
      },
      {
        input: "expression2",
        value: {
          result: {
            value: "bar",
            class: "",
          },
        },
      },
    ],
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(
    React.createElement(Expressions.WrappedComponent, props)
  );
  return { component, props };
}

describe("Expressions", () => {
  it("should render", async () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("should always have unique keys", async () => {
    const overrides = {
      expressions: [
        {
          input: "expression1",
          value: {
            result: {
              value: undefined,
              class: "",
            },
          },
        },
        {
          input: "expression2",
          value: {
            result: {
              value: undefined,
              class: "",
            },
          },
        },
      ],
    };

    const { component } = render(overrides);
    expect(component).toMatchSnapshot();
  });
});
