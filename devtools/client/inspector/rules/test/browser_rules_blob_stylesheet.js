/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct for stylesheet generated
// with createObjectURL(cssBlob)
const TEST_URL = URL_ROOT + "doc_blob_stylesheet.html";

add_task(function* () {
  yield addTab(TEST_URL);
  let {inspector, view} = yield openRuleView();

  yield selectNode("h1", inspector);
  is(view.element.querySelectorAll("#noResults").length, 0,
    "The no-results element is not displayed");

  is(view.element.querySelectorAll(".ruleview-rule").length, 2,
    "There are 2 displayed rules");
});
