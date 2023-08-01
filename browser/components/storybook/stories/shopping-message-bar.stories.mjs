/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/shopping/content/shopping-message-bar.mjs";

export default {
  title: "Domain-specific UI Widgets/Shopping/Shopping Message Bar",
  component: "shopping-message-bar",
  argTypes: {
    type: {
      control: {
        type: "select",
        options: ["stale"],
      },
    },
  },
  parameters: {
    status: "in-development",
    fluent: `
shopping-message-bar-warning-stale-analysis-title = Updates available
shopping-message-bar-warning-stale-analysis-message = Re-analyze the reviews for this product, so you have the latest info.
shopping-message-bar-warning-stale-analysis-link = Re-analyze reviews
    `,
  },
};

const Template = ({ type }) => html`
  <shopping-message-bar type=${ifDefined(type)}></shopping-message-bar>
`;

export const DefaultShoppingMessageBar = Template.bind({});
DefaultShoppingMessageBar.args = {
  type: "stale",
};
