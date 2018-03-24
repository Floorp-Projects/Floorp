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
      getThumbnailPath: sandbox.spy(() => Promise.resolve(FAKE_THUMBNAIL_PATH))
    });
    globals.set("PrivateBrowsingUtils", {isWindowPrivate: sandbox.spy(() => false)});
    testFile = [0];
    globals.set("OS", {File: {open: sandbox.spy(() => Promise.resolve({read: () => testFile, close: () => {}}))}});
    globals.set("FileUtils", {File: sandbox.spy(() => {})});
    globals.set("MIMEService", {getTypeFromFile: sandbox.spy(() => {})});
    globals.set("Services", fakeServices);
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
    it("should call OS.File.open with the correct params", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledOnce(global.OS.File.open);
    });
    it("should call FileUtils.File", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledOnce(global.FileUtils.File);
    });
    it("should call MIMEService.getTypeFromFile", async () => {
      await Screenshots.getScreenshotForURL(URL);
      assert.calledOnce(global.MIMEService.getTypeFromFile);
    });
    it("should get null if something goes wrong", async () => {
      globals.set("BackgroundPageThumbs", {captureIfMissing: () => new Error("Cannot capture tumbnail")});

      const screenshot = await Screenshots.getScreenshotForURL(URL);

      assert.calledOnce(global.PageThumbs._store);
      assert.equal(screenshot, null);
    });
    it("should get null without storing if existing thumbnail is empty", async () => {
      testFile.length = 0;

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

  describe("#_bytesToString", () => {
    it("should convert no bytes to empty string", () => {
      assert.equal(Screenshots._bytesToString([]), "");
    });
    it("should convert bytes to a string", () => {
      const str = Screenshots._bytesToString([104, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]);

      assert.equal(str, "hello world");
    });
    it("should convert very many bytes to a long string", () => {
      const bytes = [];
      for (let i = 0; i < 1000 * 1000; i++) {
        bytes.push(9);
      }

      const str = Screenshots._bytesToString(bytes);

      assert.propertyVal(str, 0, "\t");
      assert.propertyVal(str, "length", 1000000);
      assert.propertyVal(str, 999999, "\u0009");
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
