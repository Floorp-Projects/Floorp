/* eslint max-nested-callbacks: ["error", 7] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { mount } from "enzyme";
import { ConditionalPanel } from "../ConditionalPanel";
import * as mocks from "../../../utils/test-mockup";

const source = mocks.makeMockSource();

function generateDefaults(overrides, log, line, column, condition, logValue) {
  const breakpoint = mocks.makeMockBreakpoint(source, line, column);
  breakpoint.options.condition = condition;
  breakpoint.options.logValue = logValue;

  return {
    editor: {
      CodeMirror: {
        fromTextArea: jest.fn(() => {
          return {
            on: jest.fn(),
            getWrapperElement: jest.fn(() => {
              return {
                addEventListener: jest.fn(),
              };
            }),
            focus: jest.fn(),
            setCursor: jest.fn(),
            lineCount: jest.fn(),
          };
        }),
      },
      codeMirror: {
        addLineWidget: jest.fn(),
      },
    },
    location: breakpoint.location,
    source,
    breakpoint,
    log,
    getDefaultValue: jest.fn(),
    openConditionalPanel: jest.fn(),
    closeConditionalPanel: jest.fn(),
    ...overrides,
  };
}

function render(log, line, column, condition, logValue, overrides = {}) {
  const defaults = generateDefaults(
    overrides,
    log,
    line,
    column,
    condition,
    logValue
  );
  const props = { ...defaults, ...overrides };
  const wrapper = mount(React.createElement(ConditionalPanel, props));
  return { wrapper, props };
}

describe("ConditionalPanel", () => {
  it("it should render at location of selected breakpoint", () => {
    const { wrapper } = render(false, 2, 2);
    expect(wrapper).toMatchSnapshot();
  });
  it("it should render with condition at selected breakpoint location", () => {
    const { wrapper } = render(false, 3, 3, "I'm a condition", "not a log");
    expect(wrapper).toMatchSnapshot();
  });
  it("it should render with logpoint at selected breakpoint location", () => {
    const { wrapper } = render(true, 4, 4, "not a condition", "I'm a log");
    expect(wrapper).toMatchSnapshot();
  });
});
