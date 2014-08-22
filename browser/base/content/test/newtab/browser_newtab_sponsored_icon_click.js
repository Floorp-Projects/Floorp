/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  yield setLinks("0");
  yield addNewTabPageTab();

  let site = getCell(0).node.querySelector(".newtab-site");
  site.setAttribute("type", "sponsored");

  // test explain text appearing upon a click
  let sponsoredButton = site.querySelector(".newtab-sponsored");
  EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, getContentWindow());
  let explain = site.querySelector(".sponsored-explain");
  isnot(explain, null, "Sponsored explanation shown");
  ok(explain.querySelector("input").classList.contains("newtab-control-block"), "sponsored tiles show blocked image");
  ok(sponsoredButton.hasAttribute("active"), "Sponsored button has active attribute");

  // test dismissing sponsored explain
  EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, getContentWindow());
  is(site.querySelector(".sponsored-explain"), null, "Sponsored explanation no longer shown");
  ok(!sponsoredButton.hasAttribute("active"), "Sponsored button does not have active attribute");

  // test with enhanced tile
  site.setAttribute("type", "enhanced");
  EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, getContentWindow());
  explain = site.querySelector(".sponsored-explain");
  isnot(explain, null, "Sponsored explanation shown");
  ok(explain.querySelector("input").classList.contains("newtab-customize"), "enhanced tiles show customize image");
  ok(sponsoredButton.hasAttribute("active"), "Sponsored button has active attribute");

  // test dismissing enhanced explain
  EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, getContentWindow());
  is(site.querySelector(".sponsored-explain"), null, "Sponsored explanation no longer shown");
  ok(!sponsoredButton.hasAttribute("active"), "Sponsored button does not have active attribute");
}
