/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { mount } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const KeyboardCheckClass = require("devtools/client/accessibility/components/KeyboardCheck");
const KeyboardCheck = createFactory(KeyboardCheckClass);

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const {
  testCustomCheck,
} = require("devtools/client/accessibility/test/jest/helpers");

const {
  accessibility: {
    AUDIT_TYPE: { KEYBOARD },
    ISSUE_TYPE: {
      [KEYBOARD]: { INTERACTIVE_NO_ACTION, FOCUSABLE_NO_SEMANTICS },
    },
    SCORES: { FAIL, WARNING },
  },
} = require("devtools/shared/constants");

describe("KeyboardCheck component:", () => {
  const testProps = [
    { score: FAIL, issue: INTERACTIVE_NO_ACTION },
    { score: WARNING, issue: FOCUSABLE_NO_SEMANTICS },
  ];

  for (const props of testProps) {
    it(`${props.score} render`, () => {
      const wrapper = mount(
        LocalizationProvider({ bundles: [] }, KeyboardCheck(props))
      );

      const keyboardCheck = wrapper.find(KeyboardCheckClass);
      testCustomCheck(keyboardCheck, props);
    });
  }
});
