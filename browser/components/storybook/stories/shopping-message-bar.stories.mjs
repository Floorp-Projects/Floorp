/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/shopping/content/shopping-message-bar.mjs";

window.MozXULElement.insertFTLIfNeeded("browser/shopping.ftl");
window.MozXULElement.insertFTLIfNeeded("toolkit/branding/brandings.ftl");

export default {
  title: "Domain-specific UI Widgets/Shopping/Shopping Message Bar",
  component: "shopping-message-bar",
  argTypes: {
    type: {
      control: {
        type: "select",
        options: [
          "stale",
          "generic-error",
          "not-enough-reviews",
          "product-not-available",
          "product-not-available-reported",
          "thanks-for-reporting",
          "analysis-in-progress",
          "reanalysis-in-progress",
          "page-not-supported",
          "thank-you-for-feedback",
        ],
      },
    },
  },
  parameters: {
    status: "in-development",
    actions: {
      handles: ["click"],
    },
  },
};

const Template = ({ type, progress }) => html`
  <shopping-message-bar
    type=${ifDefined(type)}
    progress=${ifDefined(progress)}
  ></shopping-message-bar>
`;

export const DefaultShoppingMessageBar = Template.bind({});
DefaultShoppingMessageBar.args = {
  type: "stale",
  progress: 0,
};
