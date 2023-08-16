// If this fails it could be because of schema changes.
// `ds_layout.json` defines the newtab page format
// `topstories.json` defines the stories shown
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.discoverystream.config",
      JSON.stringify({
        api_key_pref: "extensions.pocket.oAuthConsumerKey",
        collapsible: true,
        enabled: true,
        show_spocs: false,
        hardcoded_layout: false,
        personalized: true,
        layout_endpoint:
          "https://example.com/browser/browser/components/newtab/test/browser/ds_layout.json",
      }),
    ]);
    await pushPrefs([
      "browser.newtabpage.activity-stream.discoverystream.endpoints",
      "https://example.com",
    ]);
  },
  test: async function test_card_render() {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelectorAll(
          "[data-section-id='topstories'] .ds-card-link"
        ).length
    );
    let found = content.document.querySelectorAll(
      "[data-section-id='topstories'] .ds-card-link"
    ).length;
    is(found, 1, "there should be 1 topstory card");
    let cardPublisher = content.document.querySelector(
      "[data-section-id='topstories'] .source"
    ).innerText;
    is(
      cardPublisher,
      "bbc",
      `Card publisher is ${cardPublisher} instead of bbc`
    );
  },
});
