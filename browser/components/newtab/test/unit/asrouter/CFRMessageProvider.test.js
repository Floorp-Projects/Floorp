import {CFRMessageProvider} from "lib/CFRMessageProvider.jsm";
const messages = CFRMessageProvider.getMessages();

const REGULAR_IDS = [
  "FACEBOOK_CONTAINER",
  "GOOGLE_TRANSLATE",
  "YOUTUBE_ENHANCE",
  "WIKIPEDIA_CONTEXT_MENU_SEARCH",
  "REDDIT_ENHANCEMENT",
];

describe("CFRMessageProvider", () => {
  it("should have a total of 12 messages", () => {
    assert.lengthOf(messages, 12);
  });
  it("should two variants for each of the five regular addons", () => {
    for (const id of REGULAR_IDS) {
      const cohort1 = messages.find(msg => msg.id === `${id}_1`);
      assert.ok(cohort1, `contains one day cohort for ${id}`);
      assert.deepEqual(cohort1.frequency, {lifetime: 1}, "one day cohort has the right frequency cap");
      assert.include(cohort1.targeting, `(providerCohorts.cfr in ["one_per_day", "nightly"])`);

      const cohort3 = messages.find(msg => msg.id === `${id}_3`);
      assert.ok(cohort3, `contains three day cohort for ${id}`);
      assert.deepEqual(cohort3.frequency, {lifetime: 3}, "three day cohort has the right frequency cap");
      assert.include(cohort3.targeting, `(providerCohorts.cfr == "three_per_day")`);

      assert.deepEqual(cohort1.content, cohort3.content, "cohorts should have the same content");
    }
  });
  it("should have the two amazon cohorts", () => {
    const cohort1 = messages.find(msg => msg.id === `AMAZON_ASSISTANT_1`);
    const cohort3 = messages.find(msg => msg.id === `AMAZON_ASSISTANT_3`);
    assert.deepEqual(cohort1.content, cohort3.content, "cohorts should have the same content");

    assert.ok(cohort1, `contains one day cohort for amazon`);
    assert.deepEqual(cohort1.frequency, {lifetime: 1}, "one day cohort has the right frequency cap");
    assert.include(cohort1.targeting, `(providerCohorts.cfr == "one_per_day_amazon"`);

    assert.ok(cohort3, `contains three day cohort for amazon`);
    assert.deepEqual(cohort3.frequency, {lifetime: 3}, "three day cohort has the right frequency cap");
    assert.include(cohort3.targeting, `(providerCohorts.cfr == "three_per_day_amazon")`);
  });
});
