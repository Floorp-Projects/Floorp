/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/shopping/content/letter-grade.mjs";

export default {
  title: "Domain-specific UI Widgets/Shopping/Letter Grade",
  component: "letter-grade",
  argTypes: {
    letter: {
      control: {
        type: "select",
        options: ["A", "B", "C", "D", "F"],
      },
    },
    showdescription: {
      control: {
        type: "boolean",
      },
    },
  },
  parameters: {
    status: "in-development",
    fluent: `
shopping-letter-grade-description-ab = Reliable reviews
shopping-letter-grade-description-c = Only some reliable reviews
shopping-letter-grade-description-df = Unreliable reviews
shopping-letter-grade-tooltip =
  .title = this is tooltip
    `,
  },
};

const Template = ({ letter, showdescription }) => html`
  <letter-grade
    letter=${ifDefined(letter)}
    ?showdescription=${ifDefined(showdescription)}
  ></letter-grade>
`;

export const DefaultLetterGrade = Template.bind({});
DefaultLetterGrade.args = {
  letter: "A",
  showdescription: null,
};

export const LetterGradeWithDescription = Template.bind({});
LetterGradeWithDescription.args = {
  ...DefaultLetterGrade.args,
  showdescription: true,
};
