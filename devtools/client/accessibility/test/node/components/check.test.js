/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const CheckClass = require("resource://devtools/client/accessibility/components/Check.js");
const Check = createFactory(CheckClass);

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    ISSUE_TYPE: {
      [TEXT_LABEL]: { AREA_NO_NAME_FROM_ALT },
    },
    SCORES: { FAIL },
  },
} = require("resource://devtools/shared/constants.js");

const {
  testCheck,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

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
