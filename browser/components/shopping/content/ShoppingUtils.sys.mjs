/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ShoppingUtils = {
  getHighlights() {
    return mockHighlights;
  },
};

// TODO: replace highlights fetching with API calls. For now, return mock data.
const mockHighlights = {
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
  shipping: {
    positive: [],
    negative: [],
    neutral: [],
  },
};
