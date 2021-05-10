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
  it("should have a total of 13 messages", () => {
    assert.lengthOf(messages, 13);
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
});
