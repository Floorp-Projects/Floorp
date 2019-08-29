/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const CheckClass = require("devtools/client/accessibility/components/Check");
const Check = createFactory(CheckClass);

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    ISSUE_TYPE: {
      [TEXT_LABEL]: { AREA_NO_NAME_FROM_ALT },
    },
    SCORES: { FAIL },
  },
} = require("devtools/shared/constants");

const {
  testCheck,
} = require("devtools/client/accessibility/test/jest/helpers");

describe("Check component:", () => {
  const props = {
    id: "accessibility-text-label-header",
    issue: AREA_NO_NAME_FROM_ALT,
    score: FAIL,
    getAnnotation: jest.fn(),
  };

  it("basic render", () => {
    const wrapper = mount(LocalizationProvider({ bundles: [] }, Check(props)));
    expect(wrapper.html()).toMatchSnapshot();

    testCheck(wrapper.childAt(0), {
      issue: AREA_NO_NAME_FROM_ALT,
      score: FAIL,
    });
    expect(props.getAnnotation.mock.calls.length).toBe(1);
    expect(props.getAnnotation.mock.calls[0]).toEqual([AREA_NO_NAME_FROM_ALT]);
  });
});
