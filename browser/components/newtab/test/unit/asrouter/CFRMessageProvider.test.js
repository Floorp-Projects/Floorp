import {CFRMessageProvider} from "lib/CFRMessageProvider.jsm";
const messages = CFRMessageProvider.getMessages();

const REGULAR_IDS = [
  "FACEBOOK_CONTAINER",
  "GOOGLE_TRANSLATE",
  "YOUTUBE_ENHANCE",
  // These are excluded for now.
  // "WIKIPEDIA_CONTEXT_MENU_SEARCH",
  // "REDDIT_ENHANCEMENT",
];

describe("CFRMessageProvider", () => {
  it("should have a total of 4 messages", () => {
    assert.lengthOf(messages, 4);
  });
  it("should have one message each for the three regular addons", () => {
    for (const id of REGULAR_IDS) {
      const cohort3 = messages.find(msg => msg.id === `${id}_3`);
      assert.ok(cohort3, `contains three day cohort for ${id}`);
      assert.deepEqual(cohort3.frequency, {lifetime: 3}, "three day cohort has the right frequency cap");
      assert.notInclude(cohort3.targeting, `providerCohorts.cfr`);
    }
  });
  it("should always have xpinstallEnabled as targeting if it is an addon", () => {
    for (const message of messages) {
      // Ensure that the CFR messages that are recommending an addon have this targeting.
      // In the future when we can do targeting based on category, this test will change.
      // See bug 1494778 and 1497653
      if (message.id !== "PIN_TAB") {
        assert.include(message.targeting, `(xpinstallEnabled == true)`);
      }
    }
  });
  it("should restrict all messages to `en` locale for now", () => {
    for (const message of messages) {
      if (message.id !== "PIN_TAB") {
        assert.include(message.targeting, `localeLanguageCode == "en"`);
      } else {
        assert.include(message.targeting, `locale == "en-US"`);
      }
    }
  });
});
