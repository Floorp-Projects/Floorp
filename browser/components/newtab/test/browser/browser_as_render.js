"use strict";

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
      false,
    ]);
  },
  test: function test_render_search() {
    let search = content.document.getElementById("newtab-search-text");
    ok(search, "Got the search box");
    isnot(
      search.placeholder,
      "search_web_placeholder",
      "Search box is localized"
    );
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
      true,
    ]);
  },
  test: function test_render_search_handoff() {
    let search = content.document.querySelector(".search-handoff-button");
    ok(search, "Got the search handoff button");
  },
});

test_newtab(function test_render_topsites() {
  let topSites = content.document.querySelector(".top-sites-list");
  ok(topSites, "Got the top sites section");
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.feeds.topsites",
      false,
    ]);
  },
  test: function test_render_no_topsites() {
    let topSites = content.document.querySelector(".top-sites-list");
    ok(!topSites, "No top sites section");
  },
});

// This next test runs immediately after test_render_no_topsites to make sure
// the topsites pref is restored
test_newtab(function test_render_topsites_again() {
  let topSites = content.document.querySelector(".top-sites-list");
  ok(topSites, "Got the top sites section again");
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.logowordmark.alwaysVisible",
      false,
    ]);
  },
  test: function test_render_logo_false() {
    let logoWordmark = content.document.querySelector(".logo-and-wordmark");
    ok(!logoWordmark, "The logo is not rendered when pref is false");
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.logowordmark.alwaysVisible",
      true,
    ]);
  },
  test: function test_render_logo() {
    let logoWordmark = content.document.querySelector(".logo-and-wordmark");
    ok(logoWordmark, "The logo is rendered when pref is true");
  },
});
