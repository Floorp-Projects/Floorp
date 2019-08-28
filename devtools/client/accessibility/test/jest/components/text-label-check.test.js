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

function testTextLabelCheck(wrapper, props) {
  expect(wrapper.html()).toMatchSnapshot();
  expect(wrapper.children().length).toBe(1);
  const container = wrapper.childAt(0);
  expect(container.hasClass("accessibility-check")).toBe(true);
  expect(container.prop("role")).toBe("presentation");
  expect(wrapper.props()).toMatchObject(props);

  const localized = wrapper.find(FluentReact.Localized);
  expect(localized.length).toBe(3);

  const heading = localized.at(0).childAt(0);
  expect(heading.type()).toBe("h3");
  expect(heading.hasClass("accessibility-check-header")).toBe(true);

  const icon = localized.at(1).childAt(0);
  expect(icon.type()).toBe("img");
  expect(icon.hasClass(props.score === FAIL ? "fail" : props.score)).toBe(true);

  const annotation = localized.at(2).childAt(0);
  expect(annotation.type()).toBe("p");
  expect(annotation.hasClass("accessibility-check-annotation")).toBe(true);
}

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
      testTextLabelCheck(textLabelCheck, props);
    });
  }
});
