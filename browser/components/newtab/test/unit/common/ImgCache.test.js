import {cache} from "content-src/components/DiscoveryStreamComponents/DSImage/cache.js";

describe("ImgCache", () => {
  beforeEach(() => {
    // Wipe cache
    cache.queuedImages = {};
  });

  describe("group", () => {
    it("should create multiple cache sets", () => {
      cache.query(`http://example.org/a.jpg`, 500, `1x`);
      cache.query(`http://example.org/a.jpg`, 1000, `2x`);

      assert.equal(typeof cache.queuedImages[`1x`], `object`);
      assert.equal(typeof cache.queuedImages[`2x`], `object`);
    });

    it("should cache larger values", () => {
      cache.query(`http://example.org/a.jpg`, 200, `1x`);
      cache.query(`http://example.org/a.jpg`, 500, `1x`);

      assert.equal(cache.queuedImages[`1x`][`http://example.org/a.jpg`], 500);
    });

    it("should return a larger resolution if already cached", () => {
      cache.query(`http://example.org/a.jpg`, 500, `1x`);

      let result = cache.query(`http://example.org/a.jpg`, 200, `1x`);

      assert.equal(result, 500);
    });
  });
});
