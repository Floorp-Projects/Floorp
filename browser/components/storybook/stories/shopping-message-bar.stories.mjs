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
        options: [
          "stale",
          "generic-error",
          "not-enough-reviews",
          "product-not-available",
          "offline",
          "analysis-in-progress",
        ],
      },
    },
  },
  parameters: {
    status: "in-development",
    fluent: `
shopping-message-bar-warning-stale-analysis-title = Updates available
shopping-message-bar-warning-stale-analysis-message = Re-analyze the reviews for this product, so you have the latest info.
shopping-message-bar-warning-stale-analysis-link = Re-analyze reviews
shopping-message-bar-generic-error-title = Something went wrong
shopping-message-bar-generic-error-message = Couldn't load information. Please try again later.
shopping-message-bar-warning-not-enough-reviews-title = Not enough reviews yet
shopping-message-bar-warning-not-enough-reviews-message = When this product has more reviews, we’ll be able to analyze them.
shopping-message-bar-warning-product-not-available-title = Product is not available
shopping-message-bar-warning-product-not-available-message = If you see that this product is back in stock, report it to us and we’ll work on updating the analysis.
shopping-message-bar-warning-product-not-available-button = Report this product is back in stock
shopping-message-bar-warning-offline-title = No network connection
shopping-message-bar-warning-offline-message = Check your network connection. Then, try reloading the page.
shopping-message-bar-analysis-in-progress-title = Analysis coming soon
shopping-message-bar-analysis-in-progress-message = When it’s done, we’ll automatically show the updated info here.
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
