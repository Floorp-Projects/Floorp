"use strict";

async function before({ pushPrefs }) {
  await pushPrefs([
    "browser.newtabpage.activity-stream.discoverystream.config",
    JSON.stringify({
      collapsible: true,
      enabled: true,
      hardcoded_layout: true,
    }),
  ]);
}

test_newtab({
  before,
  test: async function test_render_hardcoded() {
    const topSites = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".ds-top-sites")
    );
    ok(topSites, "Got the discovery stream top sites section");

    const learnMore = content.document.querySelector(
      ".ds-layout a[href$=new_tab_learn_more]"
    );
    is(
      learnMore.textContent,
      "How it works",
      "Got the rendered Message with link text and url within discovery stream"
    );
  },
});
