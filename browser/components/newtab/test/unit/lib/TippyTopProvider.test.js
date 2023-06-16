import { GlobalOverrider } from "test/unit/utils";
import { TippyTopProvider } from "lib/TippyTopProvider.sys.mjs";

describe("TippyTopProvider", () => {
  let instance;
  let globals;
  beforeEach(async () => {
    globals = new GlobalOverrider();
    let fetchStub = globals.sandbox.stub();
    globals.set("fetch", fetchStub);
    fetchStub.resolves({
      ok: true,
      status: 200,
      json: () =>
        Promise.resolve([
          {
            domains: ["facebook.com"],
            image_url: "images/facebook-com.png",
            favicon_url: "images/facebook-com.png",
            background_color: "#3b5998",
          },
          {
            domains: ["gmail.com", "mail.google.com"],
            image_url: "images/gmail-com.png",
            favicon_url: "images/gmail-com.png",
            background_color: "#000000",
          },
        ]),
    });
    instance = new TippyTopProvider();
    await instance.init();
  });
  it("should provide an icon for facebook.com", () => {
    const site = instance.processSite({ url: "https://facebook.com" });
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(
      site.smallFavicon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should provide an icon for www.facebook.com", () => {
    const site = instance.processSite({ url: "https://www.facebook.com" });
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(
      site.smallFavicon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should not provide an icon for other.facebook.com", () => {
    const site = instance.processSite({ url: "https://other.facebook.com" });
    assert.isUndefined(site.tippyTopIcon);
  });
  it("should provide an icon for other.facebook.com with stripping", () => {
    const site = instance.processSite(
      { url: "https://other.facebook.com" },
      "*"
    );
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
  });
  it("should provide an icon for facebook.com/foobar", () => {
    const site = instance.processSite({ url: "https://facebook.com/foobar" });
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(
      site.smallFavicon,
      "chrome://activity-stream/content/data/content/tippytop/images/facebook-com.png"
    );
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should provide an icon for gmail.com", () => {
    const site = instance.processSite({ url: "https://gmail.com" });
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/gmail-com.png"
    );
    assert.equal(
      site.smallFavicon,
      "chrome://activity-stream/content/data/content/tippytop/images/gmail-com.png"
    );
    assert.equal(site.backgroundColor, "#000000");
  });
  it("should provide an icon for mail.google.com", () => {
    const site = instance.processSite({ url: "https://mail.google.com" });
    assert.equal(
      site.tippyTopIcon,
      "chrome://activity-stream/content/data/content/tippytop/images/gmail-com.png"
    );
    assert.equal(
      site.smallFavicon,
      "chrome://activity-stream/content/data/content/tippytop/images/gmail-com.png"
    );
    assert.equal(site.backgroundColor, "#000000");
  });
  it("should handle garbage URLs gracefully", () => {
    const site = instance.processSite({ url: "garbagejlfkdsa" });
    assert.isUndefined(site.tippyTopIcon);
    assert.isUndefined(site.backgroundColor);
  });
  it("should handle error when fetching and parsing manifest", async () => {
    globals = new GlobalOverrider();
    let fetchStub = globals.sandbox.stub();
    globals.set("fetch", fetchStub);
    fetchStub.rejects("whaaaa");
    instance = new TippyTopProvider();
    await instance.init();
    instance.processSite({ url: "https://facebook.com" });
  });
});
