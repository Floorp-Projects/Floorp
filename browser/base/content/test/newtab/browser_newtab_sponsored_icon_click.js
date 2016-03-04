/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield setLinks("0");
  yield* addNewTabPageTab();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    var EventUtils = {};
    EventUtils.window = {};
    EventUtils.parent = EventUtils.window;
    EventUtils._EU_Ci = Components.interfaces;

    Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

    let cell = content.gGrid.cells[0];

    let site = cell.node.querySelector(".newtab-site");
    site.setAttribute("type", "sponsored");

    // test explain text appearing upon a click
    let sponsoredButton = site.querySelector(".newtab-sponsored");
    EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, content);
    let explain = site.querySelector(".sponsored-explain");
    Assert.notEqual(explain, null, "Sponsored explanation shown");
    Assert.ok(explain.querySelector("input").classList.contains("newtab-control-block"),
      "sponsored tiles show blocked image");
    Assert.ok(sponsoredButton.hasAttribute("active"), "Sponsored button has active attribute");

    // test dismissing sponsored explain
    EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, content);
    Assert.equal(site.querySelector(".sponsored-explain"), null,
      "Sponsored explanation no longer shown");
    Assert.ok(!sponsoredButton.hasAttribute("active"),
      "Sponsored button does not have active attribute");

    // test with enhanced tile
    site.setAttribute("type", "enhanced");
    EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, content);
    explain = site.querySelector(".sponsored-explain");
    Assert.notEqual(explain, null, "Sponsored explanation shown");
    Assert.ok(explain.querySelector("input").classList.contains("newtab-customize"),
      "enhanced tiles show customize image");
    Assert.ok(sponsoredButton.hasAttribute("active"), "Sponsored button has active attribute");

    // test dismissing enhanced explain
    EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, content);
    Assert.equal(site.querySelector(".sponsored-explain"), null,
      "Sponsored explanation no longer shown");
    Assert.ok(!sponsoredButton.hasAttribute("active"),
      "Sponsored button does not have active attribute");
  });
});
