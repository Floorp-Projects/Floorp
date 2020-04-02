/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentAPI", "TEST_REFERENCE_RECIPE"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "testSlug",
  "browser.aboutwelcome.temp.testExperiment.slug",
  ""
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "testBranch",
  "browser.aboutwelcome.temp.testExperiment.branch",
  "",
  null,
  val => val || "control"
);

// This is used by xpcshell tests
const TEST_REFERENCE_RECIPE = {
  slug: "about_welcome_test",
  branches: [
    {
      slug: "control",
      ratio: 1,
      value: {
        title: "This is the control branch",
      },
    },
    {
      slug: "variant",
      ratio: 1,
      value: {
        title: "This is the variant branch",
        subtitle: "With a subtitle.",
      },
    },
  ],
};

const PERSONAL_DATA_PROMISE_CARD = {
  id: "TRAILHEAD_CARD_12",
  content: {
    title: { string_id: "onboarding-personal-data-promise-title" },
    text: { string_id: "onboarding-personal-data-promise-text" },
    icon: "pledge",
    primary_button: {
      label: { string_id: "onboarding-personal-data-promise-button" },
      action: {
        type: "OPEN_URL",
        data: {
          args: "https://www.mozilla.org/firefox/privacy/",
          where: "tabshifted",
        },
      },
    },
  },
};

const BROWSE_PRIVATELY_CARD = {
  content: {
    title: {
      string_id: "onboarding-browse-privately-title",
    },
    text: {
      string_id: "onboarding-browse-privately-text",
    },
    icon: "private",
    primary_button: {
      label: {
        string_id: "onboarding-browse-privately-button",
      },
      action: {
        type: "OPEN_PRIVATE_BROWSER_WINDOW",
      },
    },
  },
  id: "TRAILHEAD_CARD_4",
};

const FX_MONITOR_CARD = {
  content: {
    title: {
      string_id: "onboarding-firefox-monitor-title",
    },
    text: {
      string_id: "onboarding-firefox-monitor-text2",
    },
    icon: "ffmonitor",
    primary_button: {
      label: {
        string_id: "onboarding-firefox-monitor-button",
      },
      action: {
        type: "OPEN_URL",
        data: {
          args: "https://monitor.firefox.com/",
          where: "tabshifted",
        },
      },
    },
  },
  id: "TRAILHEAD_CARD_3",
};

const SYNC_CARD = {
  content: {
    title: {
      string_id: "onboarding-data-sync-title",
    },
    text: {
      string_id: "onboarding-data-sync-text2",
    },
    icon: "devices",
    primary_button: {
      label: {
        string_id: "onboarding-data-sync-button2",
      },
      action: {
        type: "OPEN_URL",
        addFlowParams: true,
        data: {
          args:
            "https://accounts.firefox.com/?service=sync&action=email&context=fx_desktop_v3&entrypoint=activity-stream-firstrun&style=trailhead",
          where: "tabshifted",
        },
      },
    },
  },
  id: "TRAILHEAD_CARD_2",
};

const PULL_FACTOR_PRIVACY_RECIPE = {
  slug: "aw_pull_factor_privacy", // experiment id
  branches: [
    {
      slug: "control",
      ratio: 1,
      value: {},
    },
    {
      slug: "variant_1", // branch id
      ratio: 1,
      value: {
        title: "Welcome to Firefox. Fast, safe, private.",
        cards: [
          PERSONAL_DATA_PROMISE_CARD,
          FX_MONITOR_CARD,
          BROWSE_PRIVATELY_CARD,
        ],
      },
    },
    {
      slug: "variant_2",
      ratio: 1,
      value: {
        title: "Welcome to Firefox",
        cards: [
          PERSONAL_DATA_PROMISE_CARD,
          FX_MONITOR_CARD,
          BROWSE_PRIVATELY_CARD,
        ],
      },
    },
    {
      slug: "variant_3",
      ratio: 1,
      value: {
        title: "Welcome to Firefox. Fast, safe, private.",
        cards: [SYNC_CARD, FX_MONITOR_CARD, BROWSE_PRIVATELY_CARD],
      },
    },
  ],
};

const ExperimentAPI = {
  _RECIPES: [
    TEST_REFERENCE_RECIPE,
    PULL_FACTOR_PRIVACY_RECIPE,
    // Add more recipes below
  ],

  getValue() {
    const recipes = this._RECIPES;
    const experiment = testSlug && recipes.find(r => r.slug === testSlug);
    if (experiment) {
      const branch = experiment.branches.find(b => b.slug === testBranch);
      return branch ? branch.value : {};
    }
    return {};
  },
};
