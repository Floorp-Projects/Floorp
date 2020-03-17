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

const ExperimentAPI = {
  _RECIPES: [
    TEST_REFERENCE_RECIPE,
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
