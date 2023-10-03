"use strict";

async function before({ pushPrefs }) {
  await pushPrefs([
    "browser.newtabpage.activity-stream.discoverystream.config",
    JSON.stringify({
      collapsible: true,
      enabled: true,
    }),
  ]);
}

test_newtab({
  before,
  test: async function test_render_hardcoded_topsites() {
    const topSites = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".ds-top-sites")
    );
    ok(topSites, "Got the discovery stream top sites section");
  },
});

test_newtab({
  before,
  test: async function test_render_hardcoded_learnmore() {
    const learnMoreLink = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".ds-layout .learn-more-link > a")
    );
    ok(learnMoreLink, "Got the discovery stream learn more link");
  },
});
