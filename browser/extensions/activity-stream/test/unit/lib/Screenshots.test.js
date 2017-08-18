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
});
