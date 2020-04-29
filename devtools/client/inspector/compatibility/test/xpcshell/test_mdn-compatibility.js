/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for the MDN compatibility diagnosis module.

const MDNCompatibility = require("devtools/client/inspector/compatibility/lib/MDNCompatibility");
const cssPropertiesCompatData = require("devtools/client/inspector/compatibility/lib/dataset/css-properties.json");

const mdnCompatibility = new MDNCompatibility(cssPropertiesCompatData);

const FIREFOX_69 = {
  id: "firefox",
  version: "69",
};

const FIREFOX_1 = {
  id: "firefox",
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
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "grid-column",
        url: "https://developer.mozilla.org/docs/Web/CSS/grid-column",
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
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "clip",
        url: "https://developer.mozilla.org/docs/Web/CSS/clip",
        deprecated: true,
        experimental: false,
        unsupportedBrowsers: [],
      },
    ],
  },
  {
    description: "Test for a property having some issues",
    declarations: [{ name: "font-variant-alternates" }],
    browsers: [FIREFOX_1],
    expectedIssues: [
      {
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "font-variant-alternates",
        url:
          "https://developer.mozilla.org/docs/Web/CSS/font-variant-alternates",
        deprecated: true,
        experimental: false,
        unsupportedBrowsers: [FIREFOX_1],
      },
    ],
  },
  {
    description: "Test for an aliased property not supported in all browsers",
    declarations: [{ name: "-moz-user-select" }],
    browsers: [FIREFOX_69, SAFARI_13],
    expectedIssues: [
      {
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY_ALIASES,
        property: "user-select",
        aliases: ["-moz-user-select"],
        url: "https://developer.mozilla.org/docs/Web/CSS/user-select",
        deprecated: false,
        experimental: false,
        unsupportedBrowsers: [SAFARI_13],
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
