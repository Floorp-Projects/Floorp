/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that different inherited properties sections are created for rules
// inherited from several elements of the same type.

const TEST_URI = `
  <div style="cursor:pointer">
    A
    <div style="cursor:pointer">
      B<a>Cursor</a>
    </div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("a", inspector);
  yield elementStyleInherit(inspector, view);
});

function* elementStyleInherit(inspector, view) {
  let gutters = view.element.querySelectorAll(".theme-gutter");
  is(gutters.length, 2,
    "Gutters should contains 2 sections");
  ok(gutters[0].textContent, "Inherited from div");
  ok(gutters[1].textContent, "Inherited from div");
}
