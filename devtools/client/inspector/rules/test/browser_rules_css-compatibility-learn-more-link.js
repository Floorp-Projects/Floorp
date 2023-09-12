/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Learn More link is displayed when possible,
// and that it links to MDN or the spec if no MDN url is provided.

const TEST_URI = `
<style>
  body {
    user-select: none;
    hyphenate-limit-chars: auto;
    // TODO: Re-enable it when we have another property with no MDN url nor spec url Bug 1840910
    /*overflow-clip-box: padding-box;*/
  }
</style>
<body>
</body>`;

const TEST_DATA_INITIAL = [
  {
    selector: "body",
    rules: [
      {},
      {
        "user-select": {
          value: "none",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
          // MDN url
          expectedLearnMoreUrl:
            "https://developer.mozilla.org/docs/Web/CSS/user-select?utm_source=devtools&utm_medium=inspector-css-compatibility&utm_campaign=default",
        },
        "hyphenate-limit-chars": {
          value: "auto",
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
          // No MDN url, but a spec one
          expectedLearnMoreUrl:
            "https://drafts.csswg.org/css-text-4/#propdef-hyphenate-limit-chars",
        },
        // TODO: Re-enable it when we have another property with no MDN url nor spec url Bug 1840910
        /*"overflow-clip-box": {
          expected: COMPATIBILITY_TOOLTIP_MESSAGE.default,
          value: "padding-box",
          // No MDN nor spec url
          expectedLearnMoreUrl: null,
        },*/
      },
    ],
  },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  // If the test fail because the properties used are no longer in the dataset, or they
  // now have mdn/spec url although we expected them not to, uncomment the next line
  // to get all the properties in the dataset that don't have a MDN url.
  // logCssCompatDataPropertiesWithoutMDNUrl()

  await runCSSCompatibilityTests(view, inspector, TEST_DATA_INITIAL);
});
