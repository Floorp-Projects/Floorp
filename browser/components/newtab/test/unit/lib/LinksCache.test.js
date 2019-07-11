import { LinksCache } from "lib/LinksCache.jsm";

describe("LinksCache", () => {
  it("throws when failing request", async () => {
    const cache = new LinksCache();

    let rejected = false;
    try {
      await cache.request();
    } catch (error) {
      rejected = true;
    }

    assert(rejected);
  });
});
