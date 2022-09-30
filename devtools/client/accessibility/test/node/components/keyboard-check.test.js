/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { mount } = require("enzyme");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const KeyboardCheckClass = require("resource://devtools/client/accessibility/components/KeyboardCheck.js");
const KeyboardCheck = createFactory(KeyboardCheckClass);

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const {
  testCustomCheck,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const {
  accessibility: {
    AUDIT_TYPE: { KEYBOARD },
    ISSUE_TYPE: {
      [KEYBOARD]: { INTERACTIVE_NO_ACTION, FOCUSABLE_NO_SEMANTICS },
    },
    SCORES: { FAIL, WARNING },
  },
} = require("resource://devtools/shared/constants.js");

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
