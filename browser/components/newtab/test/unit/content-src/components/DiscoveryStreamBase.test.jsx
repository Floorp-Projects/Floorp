import {isAllowedCSS} from "content-src/components/DiscoveryStreamBase/DiscoveryStreamBase";

describe("<isAllowedCSS>", () => {
  it("should allow colors", () => {
    assert.isTrue(isAllowedCSS("color", "red"));
  });

  it("should allow resource urls", () => {
    assert.isTrue(isAllowedCSS("background-image", `url("resource://activity-stream/data/content/assets/glyph-info-16.svg")`));
  });

  it("should allow chrome urls", () => {
    assert.isTrue(isAllowedCSS("background-image", `url("chrome://browser/skin/history.svg")`));
  });

  it("should allow allowed https urls", () => {
    assert.isTrue(isAllowedCSS("background-image", `url("https://img-getpocket.cdn.mozilla.net/media/image.png")`));
  });

  it("should disallow other https urls", () => {
    assert.isFalse(isAllowedCSS("background-image", `url("https://mozilla.org/media/image.png")`));
  });

  it("should disallow other protocols", () => {
    assert.isFalse(isAllowedCSS("background-image", `url("ftp://mozilla.org/media/image.png")`));
  });

  it("should allow allowed multiple valid urls", () => {
    assert.isTrue(isAllowedCSS("background-image", `url("https://img-getpocket.cdn.mozilla.net/media/image.png"), url("chrome://browser/skin/history.svg")`));
  });

  it("should disallow if any invaild", () => {
    assert.isFalse(isAllowedCSS("background-image", `url("chrome://browser/skin/history.svg"), url("ftp://mozilla.org/media/image.png")`));
  });
});
