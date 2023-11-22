/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/shopping/content/shopping-container.mjs";
// Bug 1845737: These should get imported by ShoppingMessageBar
window.MozXULElement.insertFTLIfNeeded("browser/shopping.ftl");
window.MozXULElement.insertFTLIfNeeded("toolkit/branding/brandings.ftl");
// Mock for Glean
if (typeof window.Glean == "undefined") {
  window.Glean = new Proxy(() => {}, { get: () => window.Glean });
}

export default {
  title: "Domain-specific UI Widgets/Shopping/Shopping Container",
  component: "shopping-container",
  parameters: {
    status: "in-development",
  },
};

const MOCK_HIGHLIGHTS = {
  price: {
    positive: ["This watch is great and the price was even better."],
    negative: [],
    neutral: [],
  },
  quality: {
    positive: [
      "Other than that, I am very impressed with the watch and it’s capabilities.",
      "This watch performs above expectations in every way with the exception of the heart rate monitor.",
    ],
    negative: [
      "Battery life is no better than the 3 even with the solar gimmick, probably worse.",
    ],
    neutral: [
      "I have small wrists and still went with the 6X and glad I did.",
      "I can deal with the looks, as Im now retired.",
    ],
  },
  competitiveness: {
    positive: [
      "Bought this to replace my vivoactive 3.",
      "I like that this watch has so many features, especially those that monitor health like SP02, respiration, sleep, HRV status, stress, and heart rate.",
    ],
    negative: [
      "I do not use it for sleep or heartrate monitoring so not sure how accurate they are.",
    ],
    neutral: [
      "I've avoided getting a smartwatch for so long due to short battery life on most of them.",
    ],
  },
  "packaging/appearance": {
    positive: ["Great cardboard box."],
    negative: [],
    neutral: [],
  },
  shipping: {
    positive: [],
    negative: [],
    neutral: [],
  },
};

const MOCK_RECOMMENDED_ADS = [
  {
    name: "VIVO Electric 60 x 24 inch Stand Up Desk | Black Table Top, Black Frame, Height Adjustable Standing Workstation with Memory Preset Controller (DESK-KIT-1B6B)",
    url: "www.example.com",
    price: "249.99",
    currency: "USD",
    grade: "A",
    adjusted_rating: 4.6,
    sponsored: true,
    image_blob: new Blob(new Uint8Array(), { type: "image/jpeg" }),
  },
];

const Template = ({
  data,
  isOffline,
  isAnalysisInProgress,
  analysisEvent,
  productUrl,
  userReportedAvailable,
  adsEnabled,
  adsEnabledByUser,
  recommendationData,
  analysisProgress,
}) => html`
  <style>
    main {
      /**
        --shopping-header-background and sidebar background-color should
        come from shopping.page.css, but they are not loaded when
        viewing the sidebar in Storybook. Hardcode them here
       */
      --shopping-header-background: light-dark(#f9f9fb, #2b2a33);
      background-color: var(--shopping-header-background);
      width: 320px;
    }
  </style>

  <main>
    <shopping-container
      .data=${data}
      ?isOffline=${isOffline}
      ?isAnalysisInProgress=${isAnalysisInProgress}
      .analysisEvent=${analysisEvent}
      productUrl=${ifDefined(productUrl)}
      ?userReportedAvailable=${userReportedAvailable}
      ?adsEnabled=${adsEnabled}
      ?adsEnabledByUser=${adsEnabledByUser}
      .recommendationData=${recommendationData}
      analysisProgress=${analysisProgress}
    >
    </shopping-container>
  </main>
`;

export const DefaultShoppingContainer = Template.bind({});
DefaultShoppingContainer.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
};

export const NoHighlights = Template.bind({});
NoHighlights.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
  },
};

/**
 * There will be no animation if prefers-reduced-motion is enabled.
 */
export const Loading = Template.bind({});

export const UnanalyzedProduct = Template.bind({});
UnanalyzedProduct.args = {
  data: {
    adjusted_rating: null,
    grade: null,
    highlights: null,
    product_id: null,
    needs_analysis: true,
  },
};

export const NewAnalysisInProgress = Template.bind({});
NewAnalysisInProgress.args = {
  isAnalysisInProgress: true,
  analysisProgress: 15,
};

export const StaleProduct = Template.bind({});
StaleProduct.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: true,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
};

export const ReanalysisInProgress = Template.bind({});
ReanalysisInProgress.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: true,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
  isAnalysisInProgress: true,
  analysisProgress: 15,
  analysisEvent: {
    type: "ReanalysisRequested",
    productUrl: "https://example.com/ABCD123",
  },
  productUrl: "https://example.com/ABCD123",
};

/**
 * When ad functionality is enabled and the user wants the ad
 * component visible.
 */
export const AdVisibleByUser = Template.bind({});
AdVisibleByUser.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
  adsEnabled: true,
  adsEnabledByUser: true,
  recommendationData: MOCK_RECOMMENDED_ADS,
};

/**
 * When ad functionality is enabled, but the user wants the ad
 * component hidden.
 */
export const AdHiddenByUser = Template.bind({});
AdHiddenByUser.args = {
  data: {
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
  adsEnabled: true,
  adsEnabledByUser: false,
  recommendationData: MOCK_RECOMMENDED_ADS,
};

export const NotEnoughReviews = Template.bind({});
NotEnoughReviews.args = {
  data: {
    adjusted_rating: null,
    grade: null,
    highlights: null,
    product_id: "ABCD123",
    needs_analysis: true,
  },
};

export const ProductNotAvailable = Template.bind({});
ProductNotAvailable.args = {
  data: {
    deleted_product: true,
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
};

export const ThanksForReporting = Template.bind({});
ThanksForReporting.args = {
  data: {
    deleted_product: true,
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
  userReportedAvailable: true,
};

export const UnavailableProductReported = Template.bind({});
UnavailableProductReported.args = {
  data: {
    deleted_product: true,
    deleted_product_reported: true,
    product_id: "ABCD123",
    needs_analysis: false,
    adjusted_rating: 5,
    grade: "B",
    highlights: MOCK_HIGHLIGHTS,
  },
};

export const PageNotSupported = Template.bind({});
PageNotSupported.args = {
  data: {
    product_id: null,
    adjusted_rating: null,
    grade: null,
    page_not_supported: true,
  },
};

export const GenericError = Template.bind({});
GenericError.args = {
  data: {
    status: 422,
    error: "Unprocessable entity",
  },
};

export const Offline = Template.bind({});
Offline.args = {
  isOffline: true,
};
