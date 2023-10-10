// If this fails it could be because of schema changes.
// `topstories.json` defines the stories shown
test_newtab({
  async before({ pushPrefs }) {
    sinon
      .stub(DiscoveryStreamFeed.prototype, "generateFeedUrl")
      .returns(
        "https://example.com/browser/browser/components/newtab/test/browser/topstories.json"
      );
    await pushPrefs([
      "browser.newtabpage.activity-stream.discoverystream.config",
      JSON.stringify({
        api_key_pref: "extensions.pocket.oAuthConsumerKey",
        collapsible: true,
        enabled: true,
        personalized: true,
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
  async after() {
    sinon.restore();
  },
});
