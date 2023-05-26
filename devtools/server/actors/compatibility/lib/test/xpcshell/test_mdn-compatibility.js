/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for the MDN compatibility diagnosis module.

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");
const MDNCompatibility = require("resource://devtools/server/actors/compatibility/lib/MDNCompatibility.js");
const cssPropertiesCompatData = require("resource://devtools/shared/compatibility/dataset/css-properties.json");

const mdnCompatibility = new MDNCompatibility(cssPropertiesCompatData);

const FIREFOX_1 = {
  id: "firefox",
  version: "1",
};

const FIREFOX_60 = {
  id: "firefox",
  version: "60",
};

const FIREFOX_69 = {
  id: "firefox",
  version: "69",
};

const FIREFOX_ANDROID_1 = {
  id: "firefox_android",
  version: "1",
};

const SAFARI_13 = {
  id: "safari",
  version: "13",
};

const TEST_DATA = [
  {
    description: "Test for a supported property",
    declarations: [{ name: "background-color" }],
    browsers: [FIREFOX_69],
    expectedIssues: [],
  },
  {
    description: "Test for some supported properties",
    declarations: [{ name: "background-color" }, { name: "color" }],
    browsers: [FIREFOX_69],
    expectedIssues: [],
  },
  {
    description: "Test for an unsupported property",
    declarations: [{ name: "grid-column" }],
    browsers: [FIREFOX_1],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "grid-column",
        url: "https://developer.mozilla.org/docs/Web/CSS/grid-column",
        specUrl: "https://drafts.csswg.org/css-grid/#placement-shorthands",
        deprecated: false,
        experimental: false,
        unsupportedBrowsers: [FIREFOX_1],
      },
    ],
  },
  {
    description: "Test for an unknown property",
    declarations: [{ name: "unknown-property" }],
    browsers: [FIREFOX_69],
    expectedIssues: [],
  },
  {
    description: "Test for a deprecated property",
    declarations: [{ name: "clip" }],
    browsers: [FIREFOX_69],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "clip",
        url: "https://developer.mozilla.org/docs/Web/CSS/clip",
        specUrl: "https://drafts.fxtf.org/css-masking/#clip-property",
        deprecated: true,
        experimental: false,
        unsupportedBrowsers: [],
      },
    ],
  },
  {
    description: "Test for a property having some issues",
    declarations: [{ name: "ruby-align" }],
    browsers: [FIREFOX_1],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "ruby-align",
        url: "https://developer.mozilla.org/docs/Web/CSS/ruby-align",
        specUrl: "https://drafts.csswg.org/css-ruby/#ruby-align-property",
        deprecated: false,
        experimental: true,
        unsupportedBrowsers: [FIREFOX_1],
      },
    ],
  },
  {
    description:
      "Test for an aliased property not supported in all browsers with prefix needed",
    declarations: [{ name: "-moz-user-select" }],
    browsers: [FIREFOX_69, SAFARI_13],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
        property: "user-select",
        aliases: ["-moz-user-select"],
        url: "https://developer.mozilla.org/docs/Web/CSS/user-select",
        specUrl: "https://drafts.csswg.org/css-ui/#content-selection",
        deprecated: false,
        experimental: false,
        prefixNeeded: true,
        unsupportedBrowsers: [SAFARI_13],
      },
    ],
  },
  {
    description:
      "Test for an aliased property not supported in all browsers without prefix needed",
    declarations: [
      { name: "-moz-user-select" },
      { name: "-webkit-user-select" },
    ],
    browsers: [FIREFOX_ANDROID_1, FIREFOX_69, SAFARI_13],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
        property: "user-select",
        aliases: ["-moz-user-select", "-webkit-user-select"],
        url: "https://developer.mozilla.org/docs/Web/CSS/user-select",
        specUrl: "https://drafts.csswg.org/css-ui/#content-selection",
        deprecated: false,
        experimental: false,
        prefixNeeded: false,
        unsupportedBrowsers: [FIREFOX_ANDROID_1],
      },
    ],
  },
  {
    description: "Test for aliased properties supported in all browsers",
    declarations: [
      { name: "-moz-user-select" },
      { name: "-webkit-user-select" },
    ],
    browsers: [FIREFOX_69, SAFARI_13],
    expectedIssues: [],
  },
  {
    description: "Test for a property defined with prefix",
    declarations: [{ name: "-moz-user-input" }],
    browsers: [FIREFOX_1, FIREFOX_60, FIREFOX_69],
    expectedIssues: [
      {
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "-moz-user-input",
        url: "https://developer.mozilla.org/docs/Web/CSS/-moz-user-input",
        specUrl: undefined,
        deprecated: true,
        experimental: false,
        unsupportedBrowsers: [],
      },
    ],
  },
];

add_task(() => {
  for (const {
    description,
    declarations,
    browsers,
    expectedIssues,
  } of TEST_DATA) {
    info(description);
    const issues = mdnCompatibility.getCSSDeclarationBlockIssues(
      declarations,
      browsers
    );
    deepEqual(
      issues,
      expectedIssues,
      "CSS declaration compatibility data matches expectations"
    );
  }
});
