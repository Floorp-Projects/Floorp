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
    fluent: `
shopping-message-bar-warning-stale-analysis-message-2 = New info to check
shopping-message-bar-warning-stale-analysis-button = Check now
shopping-message-bar-generic-error-title2 = No info available right now
shopping-message-bar-generic-error-message = We’re working to resolve the issue. Please check back soon.
shopping-message-bar-warning-not-enough-reviews-title = Not enough reviews yet
shopping-message-bar-warning-not-enough-reviews-message2 = When this product has more reviews, we’ll be able to check their quality.
shopping-message-bar-warning-product-not-available-title = Product is not available
shopping-message-bar-warning-product-not-available-message2 = If you see this product is back in stock, report it and we’ll work on checking the reviews.
shopping-message-bar-warning-product-not-available-button = Report this product is back in stock
shopping-message-bar-thanks-for-reporting-title = Thanks for reporting!
shopping-message-bar-thanks-for-reporting-message2 = We should have info about this product’s reviews within 24 hours. Please check back.
shopping-message-bar-warning-product-not-available-reported-title2 = Info coming soon
shopping-message-bar-warning-product-not-available-reported-message2 = We should have info about this product’s reviews within 24 hours. Please check back.
shopping-message-bar-analysis-in-progress-title2 = Checking review quality
shopping-message-bar-analysis-in-progress-message2 = This could take about 60 seconds.
shopping-message-bar-page-not-supported-title = We can't check these reviews
shopping-message-bar-page-not-supported-message = Unfortunately, we can’t check the review quality for certain types of products. For example, gift cards and streaming video, music, and games.
shopping-survey-thanks-message = Thanks for your feedback!
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
