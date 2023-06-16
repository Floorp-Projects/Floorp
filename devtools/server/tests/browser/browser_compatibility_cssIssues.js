/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getNodeCssIssues

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");
const URL = MAIN_DOMAIN + "doc_compatibility.html";

const CHROME_81 = {
  id: "chrome",
  version: "81",
};

const CHROME_ANDROID = {
  id: "chrome_android",
  version: "81",
};

const EDGE_81 = {
  id: "edge",
  version: "81",
};

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

const FIREFOX_MOBILE = {
  id: "firefox_android",
  version: "68",
};

const SAFARI_13 = {
  id: "safari",
  version: "13",
};

const SAFARI_MOBILE = {
  id: "safari_ios",
  version: "13.4",
};

const TARGET_BROWSERS = [
  FIREFOX_1,
  FIREFOX_60,
  FIREFOX_69,
  FIREFOX_MOBILE,
  CHROME_81,
  CHROME_ANDROID,
  SAFARI_13,
  SAFARI_MOBILE,
  EDGE_81,
];

const ISSUE_USER_SELECT = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
  property: "user-select",
  aliases: ["-moz-user-select"],
  url: "https://developer.mozilla.org/docs/Web/CSS/user-select",
  specUrl: "https://drafts.csswg.org/css-ui/#content-selection",
  deprecated: false,
  experimental: false,
  prefixNeeded: true,
  unsupportedBrowsers: [
    CHROME_81,
    CHROME_ANDROID,
    SAFARI_13,
    SAFARI_MOBILE,
    EDGE_81,
  ],
};

const ISSUE_CLIP = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "clip",
  url: "https://developer.mozilla.org/docs/Web/CSS/clip",
  specUrl: "https://drafts.fxtf.org/css-masking/#clip-property",
  deprecated: true,
  experimental: false,
  unsupportedBrowsers: [],
};

async function testNodeCssIssues(selector, walker, compatibility, expected) {
  const node = await walker.querySelector(walker.rootNode, selector);
  const cssCompatibilityIssues = await compatibility.getNodeCssIssues(
    node,
    TARGET_BROWSERS
  );
  info("Ensure result is correct");
  Assert.deepEqual(
    cssCompatibilityIssues,
    expected,
    "Expected CSS browser compat data is correct."
  );
}

add_task(async function () {
  const { inspector, walker, target } = await initInspectorFront(URL);
  const compatibility = await inspector.getCompatibilityFront();

  info('Test CSS properties linked with the "div" tag');
  await testNodeCssIssues("div", walker, compatibility, []);

  info('Test CSS properties linked with class "class-user-select"');
  await testNodeCssIssues(".class-user-select", walker, compatibility, [
    ISSUE_USER_SELECT,
  ]);

  info("Test CSS properties linked with multiple classes and id");
  await testNodeCssIssues(
    "div#id-clip.class-clip.class-user-select",
    walker,
    compatibility,
    [ISSUE_CLIP, ISSUE_USER_SELECT]
  );

  info("Repeated incompatible CSS rule should be only reported once");
  await testNodeCssIssues(".duplicate", walker, compatibility, [ISSUE_CLIP]);

  await target.destroy();
  gBrowser.removeCurrentTab();
});
