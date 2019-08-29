/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { mount } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const TextLabelCheckClass = require("devtools/client/accessibility/components/TextLabelCheck");
const TextLabelCheck = createFactory(TextLabelCheckClass);

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const {
  testCustomCheck,
} = require("devtools/client/accessibility/test/jest/helpers");

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    ISSUE_TYPE: {
      [TEXT_LABEL]: {
        AREA_NO_NAME_FROM_ALT,
        DIALOG_NO_NAME,
        FORM_NO_VISIBLE_NAME,
      },
    },
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("devtools/shared/constants");

describe("TextLabelCheck component:", () => {
  const testProps = [
    { issue: AREA_NO_NAME_FROM_ALT, score: FAIL },
    { issue: FORM_NO_VISIBLE_NAME, score: WARNING },
    { issue: DIALOG_NO_NAME, score: BEST_PRACTICES },
  ];

  for (const props of testProps) {
    it(`${props.score} render`, () => {
      const wrapper = mount(
        LocalizationProvider({ bundles: [] }, TextLabelCheck(props))
      );

      const textLabelCheck = wrapper.find(TextLabelCheckClass);
      testCustomCheck(textLabelCheck, props);
    });
  }
});
