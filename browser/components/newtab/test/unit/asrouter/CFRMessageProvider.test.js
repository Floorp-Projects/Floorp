import { CFRMessageProvider } from "lib/CFRMessageProvider.jsm";

const REGULAR_IDS = [
  "FACEBOOK_CONTAINER",
  "GOOGLE_TRANSLATE",
  "YOUTUBE_ENHANCE",
  // These are excluded for now.
  // "WIKIPEDIA_CONTEXT_MENU_SEARCH",
  // "REDDIT_ENHANCEMENT",
];

describe("CFRMessageProvider", () => {
  let messages;
  beforeEach(async () => {
    messages = await CFRMessageProvider.getMessages();
  });
  it("should have a total of 14 messages", () => {
    assert.lengthOf(messages, 14);
  });
  it("should have one message each for the three regular addons", () => {
    for (const id of REGULAR_IDS) {
      const cohort3 = messages.find(msg => msg.id === `${id}_3`);
      assert.ok(cohort3, `contains three day cohort for ${id}`);
      assert.deepEqual(
        cohort3.frequency,
        { lifetime: 3 },
        "three day cohort has the right frequency cap"
      );
      assert.notInclude(cohort3.targeting, `providerCohorts.cfr`);
    }
  });
  it("should restrict locale for PIN_TAB message", () => {
    const pinTabMessage = messages.find(m => m.id === "PIN_TAB");

    // 6 en-* locales, fr and de
    assert.lengthOf(pinTabMessage.targeting.match(/en-|fr|de/g), 8);
  });
  it("should contain `www.` version of the hosts", () => {
    const pinTabMessage = messages.find(m => m.id === "PIN_TAB");

    assert.isTrue(
      !!pinTabMessage.trigger.params.filter(host => host.startsWith("www."))
        .length
    );
  });
});
