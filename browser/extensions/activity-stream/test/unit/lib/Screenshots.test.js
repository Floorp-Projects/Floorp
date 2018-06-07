"use strict";
import {GlobalOverrider} from "test/unit/utils";
import {Screenshots} from "lib/Screenshots.jsm";

const URL = "foo.com";
const FAKE_THUMBNAIL_PATH = "fake/path/thumb.jpg";

describe("Screenshots", () => {
  let globals;
  let sandbox;
  let fakeServices;
  let testFile;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fakeServices = {
      wm: {
        getEnumerator() {
          return {
            hasMoreElements: () => true,
            getNext: () => {}
          };
        }
      }
    };
    globals.set("BackgroundPageThumbs", {captureIfMissing: sandbox.spy(() => Promise.resolve())});
    globals.set("PageThumbs", {
      _store: sandbox.stub(),
      getThumbnailPath: sandbox.spy(() => FAKE_THUMBNAIL_PATH)
    });
    globals.set("PrivateBrowsingUtils", {isWindowPrivate: sandbox.spy(() => false)});
    testFile = {size: 1};
    globals.set("Services", fakeServices);
    globals.set("fetch", sandbox.spy(() => Promise.resolve({blob: () => Promise.resolve(testFile)})));
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#getScreenshotForURL", () => {
    it("should call BackgroundPageThumbs.captureIfMissing with the correct url", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledWith(global.BackgroundPageThumbs.captureIfMissing, URL);
    });
    it("should call PageThumbs.getThumbnailPath with the correct url", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledWith(global.PageThumbs.getThumbnailPath, URL);
    });
    it("should call fetch", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledOnce(global.fetch);
    });
    it("should have the necessary keys in the response object", async () => {
      const screenshot = await Screenshots.getScreenshotForURL(URL);

      assert.notEqual(screenshot.path, undefined);
      assert.notEqual(screenshot.data, undefined);
    });
    it("should get null if something goes wrong", async () => {
      globals.set("BackgroundPageThumbs", {captureIfMissing: () => Promise.reject(new Error("Cannot capture thumbnail"))});

      const screenshot = await Screenshots.getScreenshotForURL(URL);

      assert.calledOnce(global.PageThumbs._store);
      assert.equal(screenshot, null);
    });
    it("should get null without storing if existing thumbnail is empty", async () => {
      testFile.size = 0;

      const screenshot = await Screenshots.getScreenshotForURL(URL);

      assert.notCalled(global.PageThumbs._store);
      assert.equal(screenshot, null);
    });
  });

  describe("#maybeCacheScreenshot", () => {
    let link;
    beforeEach(() => {
      link = {__sharedCache: {updateLink: (prop, val) => { link[prop] = val; }}};
    });
    it("should call getScreenshotForURL", () => {
      sandbox.stub(Screenshots, "getScreenshotForURL");
      sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(true);
      Screenshots.maybeCacheScreenshot(link, "mozilla.com", "image", sinon.stub());

      assert.calledOnce(Screenshots.getScreenshotForURL);
      assert.calledWithExactly(Screenshots.getScreenshotForURL, "mozilla.com");
    });
    it("should not call getScreenshotForURL twice if a fetch is in progress", () => {
      sandbox.stub(Screenshots, "getScreenshotForURL").returns(new Promise(() => {}));
      sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(true);
      Screenshots.maybeCacheScreenshot(link, "mozilla.com", "image", sinon.stub());
      Screenshots.maybeCacheScreenshot(link, "mozilla.org", "image", sinon.stub());

      assert.calledOnce(Screenshots.getScreenshotForURL);
      assert.calledWithExactly(Screenshots.getScreenshotForURL, "mozilla.com");
    });
    it("should not call getScreenshotsForURL if property !== undefined", async () => {
      sandbox.stub(Screenshots, "getScreenshotForURL").returns(Promise.resolve(null));
      sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(true);
      await Screenshots.maybeCacheScreenshot(link, "mozilla.com", "image", sinon.stub());
      await Screenshots.maybeCacheScreenshot(link, "mozilla.org", "image", sinon.stub());

      assert.calledOnce(Screenshots.getScreenshotForURL);
      assert.calledWithExactly(Screenshots.getScreenshotForURL, "mozilla.com");
    });
    it("should check if we are in private browsing before getting screenshots", async () => {
      sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(true);
      await Screenshots.maybeCacheScreenshot(link, "mozilla.com", "image", sinon.stub());

      assert.calledOnce(Screenshots._shouldGetScreenshots);
    });
    it("should not get a screenshot if we are in private browsing", async () => {
      sandbox.stub(Screenshots, "getScreenshotForURL");
      sandbox.stub(Screenshots, "_shouldGetScreenshots").returns(false);
      await Screenshots.maybeCacheScreenshot(link, "mozilla.com", "image", sinon.stub());

      assert.notCalled(Screenshots.getScreenshotForURL);
    });
  });

  describe("#_shouldGetScreenshots", () => {
    beforeEach(() => {
      let more = 2;
      sandbox.stub(global.Services.wm, "getEnumerator").returns({getNext: () => {}, hasMoreElements() { return more--; }});
    });
    it("should use private browsing utils to determine if a window is private", () => {
      Screenshots._shouldGetScreenshots();
      assert.calledOnce(global.PrivateBrowsingUtils.isWindowPrivate);
    });
    it("should return true if there exists at least 1 non-private window", () => {
      assert.isTrue(Screenshots._shouldGetScreenshots());
    });
    it("should return false if there exists private windows", () => {
      global.PrivateBrowsingUtils = {isWindowPrivate: sandbox.spy(() => true)};
      assert.isFalse(Screenshots._shouldGetScreenshots());
      assert.calledTwice(global.PrivateBrowsingUtils.isWindowPrivate);
    });
  });
});
