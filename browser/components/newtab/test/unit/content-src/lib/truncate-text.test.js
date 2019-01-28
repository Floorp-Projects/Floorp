import {truncateText} from "content-src/lib/truncate-text";

describe("truncateText", () => {
  it("should accept nothing", () => {
    assert.equal(truncateText(), "");
  });

  it("should give back string with no truncating", () => {
    const str = "hello";

    assert.equal(truncateText(str), str);
  });

  it("should give back short string for long cap", () => {
    const str = "hello";

    assert.equal(truncateText(str, 100), str);
  });

  it("should give back string for exact cap", () => {
    const str = "hello";

    assert.equal(truncateText(str, str.length), str);
  });

  it("should cap off long string with ellipsis", () => {
    const str = "hello world";

    assert.equal(truncateText(str, 5), "hello…");
  });

  it("should avoid putting ellipsis after whitespace", () => {
    const str = "hello             world";

    assert.equal(truncateText(str, 10), "hello…");
  });
});
