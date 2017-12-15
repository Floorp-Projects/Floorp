"use strict";
const {TippyTopProvider} = require("lib/TippyTopProvider.jsm");
const {GlobalOverrider} = require("test/unit/utils");

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
      json: () => Promise.resolve([{
        "title": "facebook",
        "url": "https://www.facebook.com/",
        "image_url": "facebook-com.png",
        "background_color": "#3b5998",
        "domain": "facebook.com"
      }, {
        "title": "gmail",
        "urls": ["https://www.gmail.com/", "https://mail.google.com"],
        "image_url": "gmail-com.png",
        "background_color": "#000000",
        "domain": "gmail.com"
      }])
    });
    instance = new TippyTopProvider();
    await instance.init();
  });
  it("should provide an icon for facebook.com", () => {
    const site = instance.processSite({url: "https://facebook.com"});
    assert.equal(site.tippyTopIcon, "resource://activity-stream/data/content/tippytop/images/facebook-com.png");
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should provide an icon for www.facebook.com", () => {
    const site = instance.processSite({url: "https://www.facebook.com"});
    assert.equal(site.tippyTopIcon, "resource://activity-stream/data/content/tippytop/images/facebook-com.png");
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should provide an icon for facebook.com/foobar", () => {
    const site = instance.processSite({url: "https://facebook.com/foobar"});
    assert.equal(site.tippyTopIcon, "resource://activity-stream/data/content/tippytop/images/facebook-com.png");
    assert.equal(site.backgroundColor, "#3b5998");
  });
  it("should provide an icon for gmail.com", () => {
    const site = instance.processSite({url: "https://gmail.com"});
    assert.equal(site.tippyTopIcon, "resource://activity-stream/data/content/tippytop/images/gmail-com.png");
    assert.equal(site.backgroundColor, "#000000");
  });
  it("should provide an icon for mail.google.com", () => {
    const site = instance.processSite({url: "https://mail.google.com"});
    assert.equal(site.tippyTopIcon, "resource://activity-stream/data/content/tippytop/images/gmail-com.png");
    assert.equal(site.backgroundColor, "#000000");
  });
  it("should handle garbage URLs gracefully", () => {
    const site = instance.processSite({url: "garbagejlfkdsa"});
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
    instance.processSite({url: "https://facebook.com"});
  });
});
