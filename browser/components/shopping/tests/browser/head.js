/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/shopping/test/browser/head.js",
  this
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const MOCK_UNPOPULATED_DATA = {
  adjusted_rating: null,
  grade: null,
  highlights: null,
};

const MOCK_POPULATED_DATA = {
  adjusted_rating: 5,
  grade: "B",
  highlights: {
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
  },
};

const MOCK_INVALID_KEY_OBJ = {
  invalidHighlight: {
    negative: ["This is an invalid highlight and should not be visible"],
  },
  shipping: {
    positive: [],
    negative: [],
    neutral: [],
  },
};

const MOCK_UNANALYZED_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: null,
  needs_analysis: true,
};

const MOCK_STALE_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  grade: "A",
  needs_analysis: true,
};

const MOCK_UNGRADED_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: true,
};

const MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: false,
  not_enough_reviews: true,
};

const MOCK_ANALYZED_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: false,
};

const MOCK_UNAVAILABLE_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  deleted_product: true,
};

const MOCK_UNAVAILABLE_PRODUCT_REPORTED_RESPONSE = {
  ...MOCK_UNAVAILABLE_PRODUCT_RESPONSE,
  deleted_product_reported: true,
};

const MOCK_PAGE_NOT_SUPPORTED_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  page_not_supported: true,
};

const MOCK_RECOMMENDED_ADS_RESPONSE = [
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

function verifyAnalysisDetailsVisible(shoppingContainer) {
  ok(
    shoppingContainer.reviewReliabilityEl,
    "review-reliability should be visible"
  );
  ok(shoppingContainer.adjustedRatingEl, "adjusted-rating should be visible");
  ok(shoppingContainer.highlightsEl, "review-highlights should be visible");
}

function verifyAnalysisDetailsHidden(shoppingContainer) {
  ok(
    !shoppingContainer.reviewReliabilityEl,
    "review-reliability should not be visible"
  );
  ok(
    !shoppingContainer.adjustedRatingEl,
    "adjusted-rating should not be visible"
  );
  ok(
    !shoppingContainer.highlightsEl,
    "review-highlights should not be visible"
  );
}

function verifyFooterVisible(shoppingContainer) {
  ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
  ok(
    shoppingContainer.analysisExplainerEl,
    "Got the analysis-explainer element"
  );
}

function verifyFooterHidden(shoppingContainer) {
  ok(!shoppingContainer.settingsEl, "Do not render shopping-settings element");
  ok(
    !shoppingContainer.analysisExplainerEl,
    "Do not render the analysis-explainer element"
  );
}

function getAnalysisDetails(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;
    let returnState = {};
    for (let el of [
      "unanalyzedProductEl",
      "reviewReliabilityEl",
      "analysisExplainerEl",
      "adjustedRatingEl",
      "highlightsEl",
      "settingsEl",
      "shoppingMessageBarEl",
      "loadingEl",
    ]) {
      returnState[el] =
        !!shoppingContainer[el] &&
        ContentTaskUtils.isVisible(shoppingContainer[el]);
    }
    returnState.shoppingMessageBarType =
      shoppingContainer.shoppingMessageBarEl?.getAttribute("type");
    returnState.isOffline = shoppingContainer.isOffline;
    return returnState;
  });
}

function getSettingsDetails(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;
    let shoppingSettings = shoppingContainer.settingsEl;
    await shoppingSettings.updateComplete;
    let returnState = {
      settingsEl:
        !!shoppingSettings && ContentTaskUtils.isVisible(shoppingSettings),
    };
    for (let el of ["recommendationsToggleEl", "optOutButtonEl"]) {
      returnState[el] =
        !!shoppingSettings[el] &&
        ContentTaskUtils.isVisible(shoppingSettings[el]);
    }
    return returnState;
  });
}
