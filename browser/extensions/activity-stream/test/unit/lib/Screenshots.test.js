"use strict";
const {Screenshots} = require("lib/Screenshots.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const URL = "foo.com";
const FAKE_THUMBNAIL_PATH = "fake/path/thumb.jpg";

describe("Screenshots", () => {
  let globals;
  let sandbox;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("BackgroundPageThumbs", {captureIfMissing: sandbox.spy(() => Promise.resolve())});
    globals.set("PageThumbs", {getThumbnailPath: sandbox.spy(() => Promise.resolve(FAKE_THUMBNAIL_PATH))});
    globals.set("OS", {File: {open: sandbox.spy(() => Promise.resolve({read: () => [], close: () => {}}))}});
    globals.set("FileUtils", {File: sandbox.spy(() => {})});
    globals.set("MIMEService", {getTypeFromFile: sandbox.spy(() => {})});
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
    it("should throw if something goes wrong", async () => {
      globals.set("BackgroundPageThumbs", {captureIfMissing: () => new Error("Cannot capture tumbnail")});
      const screenshot = await Screenshots.getScreenshotForURL(URL);
      assert.equal(screenshot, null);
    });
  });
});
