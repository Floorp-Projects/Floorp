/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

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
    shipping: {
      positive: [],
      negative: [],
      neutral: [],
    },
  },
};

const MOCK_ERROR_OBJ = {
  status: 422,
  error: "Unprocessable entity",
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
