import { actionTypes as at } from "common/Actions.jsm";
import { DownloadsManager } from "lib/DownloadsManager.jsm";
import { GlobalOverrider } from "test/unit/utils";

describe("Downloads Manager", () => {
  let downloadsManager;
  let globals;
  const DOWNLOAD_URL = "https://site.com/download.mov";

  beforeEach(() => {
    globals = new GlobalOverrider();
    global.Cc["@mozilla.org/timer;1"] = {
      createInstance() {
        return {
          initWithCallback: sinon.stub().callsFake(callback => callback()),
          cancel: sinon.spy(),
        };
      },
    };

    globals.set("DownloadsCommon", {
      getData: sinon.stub().returns({
        addView: sinon.stub(),
        removeView: sinon.stub(),
      }),
      copyDownloadLink: sinon.stub(),
      deleteDownload: sinon.stub().returns(Promise.resolve()),
      openDownload: sinon.stub(),
      showDownloadedFile: sinon.stub(),
    });

    downloadsManager = new DownloadsManager();
    downloadsManager.init({ dispatch() {} });
    downloadsManager.onDownloadAdded({
      source: { url: DOWNLOAD_URL },
      endTime: Date.now(),
      target: { path: "/path/to/download.mov", exists: true },
      succeeded: true,
      refresh: async () => {},
    });
    assert.ok(downloadsManager._downloadItems.has(DOWNLOAD_URL));

    globals.set("NewTabUtils", { blockedLinks: { isBlocked() {} } });
  });
  afterEach(() => {
    downloadsManager._downloadItems.clear();
    globals.restore();
  });
  describe("#init", () => {
    it("should add a DownloadsCommon view on init", () => {
      downloadsManager.init({ dispatch() {} });
      assert.calledTwice(global.DownloadsCommon.getData().addView);
    });
  });
  describe("#onAction", () => {
    it("should copy the file on COPY_DOWNLOAD_LINK", () => {
      downloadsManager.onAction({
        type: at.COPY_DOWNLOAD_LINK,
        data: { url: DOWNLOAD_URL },
      });
      assert.calledOnce(global.DownloadsCommon.copyDownloadLink);
    });
    it("should remove the file on REMOVE_DOWNLOAD_FILE", () => {
      downloadsManager.onAction({
        type: at.REMOVE_DOWNLOAD_FILE,
        data: { url: DOWNLOAD_URL },
      });
      assert.calledOnce(global.DownloadsCommon.deleteDownload);
    });
    it("should show the file on SHOW_DOWNLOAD_FILE", () => {
      downloadsManager.onAction({
        type: at.SHOW_DOWNLOAD_FILE,
        data: { url: DOWNLOAD_URL },
      });
      assert.calledOnce(global.DownloadsCommon.showDownloadedFile);
    });
    it("should open the file on OPEN_DOWNLOAD_FILE if the type is download", () => {
      downloadsManager.onAction({
        type: at.OPEN_DOWNLOAD_FILE,
        data: { url: DOWNLOAD_URL, type: "download" },
        _target: { browser: {} },
      });
      assert.calledOnce(global.DownloadsCommon.openDownload);
    });
    it("should copy the file on UNINIT", () => {
      // DownloadsManager._downloadData needs to exist first
      downloadsManager.onAction({ type: at.UNINIT });
      assert.calledOnce(global.DownloadsCommon.getData().removeView);
    });
    it("should not execute a download command if we do not have the correct url", () => {
      downloadsManager.onAction({
        type: at.SHOW_DOWNLOAD_FILE,
        data: { url: "unknown_url" },
      });
      assert.notCalled(global.DownloadsCommon.showDownloadedFile);
    });
  });
  describe("#onDownloadAdded", () => {
    let newDownload;
    beforeEach(() => {
      downloadsManager._downloadItems.clear();
      newDownload = {
        source: { url: "https://site.com/newDownload.mov" },
        endTime: Date.now(),
        target: { path: "/path/to/newDownload.mov", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
    });
    afterEach(() => {
      downloadsManager._downloadItems.clear();
    });
    it("should add a download on onDownloadAdded", () => {
      downloadsManager.onDownloadAdded(newDownload);
      assert.ok(
        downloadsManager._downloadItems.has("https://site.com/newDownload.mov")
      );
    });
    it("should not add a download if it already exists", () => {
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(newDownload);
      const results = downloadsManager._downloadItems;
      assert.equal(results.size, 1);
    });
    it("should not return any downloads if no threshold is provided", async () => {
      downloadsManager.onDownloadAdded(newDownload);
      const results = await downloadsManager.getDownloads(null, {});
      assert.equal(results.length, 0);
    });
    it("should stop at numItems when it found one it's looking for", async () => {
      const aDownload = {
        source: { url: "https://site.com/aDownload.pdf" },
        endTime: Date.now(),
        target: { path: "/path/to/aDownload.pdf", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(aDownload);
      downloadsManager.onDownloadAdded(newDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 1,
        onlySucceeded: true,
        onlyExists: true,
      });
      assert.equal(results.length, 1);
      assert.equal(results[0].url, aDownload.source.url);
    });
    it("should get all the downloads younger than the threshold provided", async () => {
      const oldDownload = {
        source: { url: "https://site.com/oldDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/oldDownload.pdf", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
      // Add an old download (older than 36 hours in this case)
      downloadsManager.onDownloadAdded(oldDownload);
      downloadsManager.onDownloadAdded(newDownload);
      const RECENT_DOWNLOAD_THRESHOLD = 36 * 60 * 60 * 1000;
      const results = await downloadsManager.getDownloads(
        RECENT_DOWNLOAD_THRESHOLD,
        { numItems: 5, onlySucceeded: true, onlyExists: true }
      );
      assert.equal(results.length, 1);
      assert.equal(results[0].url, newDownload.source.url);
    });
    it("should dispatch DOWNLOAD_CHANGED when adding a download", () => {
      downloadsManager._store.dispatch = sinon.spy();
      downloadsManager._downloadTimer = null; // Nuke the timer
      downloadsManager.onDownloadAdded(newDownload);
      assert.calledOnce(downloadsManager._store.dispatch);
    });
    it("should refresh the downloads if onlyExists is true", async () => {
      const aDownload = {
        source: { url: "https://site.com/aDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/aDownload.pdf", exists: true },
        succeeded: true,
        refresh: () => {},
      };
      sinon.stub(aDownload, "refresh").returns(Promise.resolve());
      downloadsManager.onDownloadAdded(aDownload);
      await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
        onlyExists: true,
      });
      assert.calledOnce(aDownload.refresh);
    });
    it("should not refresh the downloads if onlyExists is false (by default)", async () => {
      const aDownload = {
        source: { url: "https://site.com/aDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/aDownload.pdf", exists: true },
        succeeded: true,
        refresh: () => {},
      };
      sinon.stub(aDownload, "refresh").returns(Promise.resolve());
      downloadsManager.onDownloadAdded(aDownload);
      await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
      });
      assert.notCalled(aDownload.refresh);
    });
    it("should only return downloads that exist if specified", async () => {
      const nonExistantDownload = {
        source: { url: "https://site.com/nonExistantDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/nonExistantDownload.pdf", exists: false },
        succeeded: true,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(nonExistantDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
        onlyExists: true,
      });
      assert.equal(results.length, 1);
      assert.equal(results[0].url, newDownload.source.url);
    });
    it("should return all downloads that either exist or don't exist if not specified", async () => {
      const nonExistantDownload = {
        source: { url: "https://site.com/nonExistantDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/nonExistantDownload.pdf", exists: false },
        succeeded: true,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(nonExistantDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
      });
      assert.equal(results.length, 2);
      assert.equal(results[0].url, newDownload.source.url);
      assert.equal(results[1].url, nonExistantDownload.source.url);
    });
    it("should return only unblocked downloads", async () => {
      const nonExistantDownload = {
        source: { url: "https://site.com/nonExistantDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/nonExistantDownload.pdf", exists: false },
        succeeded: true,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(nonExistantDownload);
      globals.set("NewTabUtils", {
        blockedLinks: {
          isBlocked: item => item.url === nonExistantDownload.source.url,
        },
      });

      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
      });

      assert.equal(results.length, 1);
      assert.propertyVal(results[0], "url", newDownload.source.url);
    });
    it("should only return downloads that were successful if specified", async () => {
      const nonSuccessfulDownload = {
        source: { url: "https://site.com/nonSuccessfulDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/nonSuccessfulDownload.pdf", exists: false },
        succeeded: false,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(nonSuccessfulDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
      });
      assert.equal(results.length, 1);
      assert.equal(results[0].url, newDownload.source.url);
    });
    it("should return all downloads that were either successful or not if not specified", async () => {
      const nonExistantDownload = {
        source: { url: "https://site.com/nonExistantDownload.pdf" },
        endTime: Date.now() - 40 * 60 * 60 * 1000,
        target: { path: "/path/to/nonExistantDownload.pdf", exists: true },
        succeeded: false,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
      downloadsManager.onDownloadAdded(nonExistantDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
      });
      assert.equal(results.length, 2);
      assert.equal(results[0].url, newDownload.source.url);
      assert.equal(results[1].url, nonExistantDownload.source.url);
    });
    it("should sort the downloads by recency", async () => {
      const olderDownload1 = {
        source: { url: "https://site.com/oldDownload1.pdf" },
        endTime: Date.now() - 2 * 60 * 60 * 1000, // 2 hours ago
        target: { path: "/path/to/oldDownload1.pdf", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
      const olderDownload2 = {
        source: { url: "https://site.com/oldDownload2.pdf" },
        endTime: Date.now() - 60 * 60 * 1000, // 1 hour ago
        target: { path: "/path/to/oldDownload2.pdf", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
      // Add some older downloads and check that they are in order
      downloadsManager.onDownloadAdded(olderDownload1);
      downloadsManager.onDownloadAdded(olderDownload2);
      downloadsManager.onDownloadAdded(newDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
        onlyExists: true,
      });
      assert.equal(results.length, 3);
      assert.equal(results[0].url, newDownload.source.url);
      assert.equal(results[1].url, olderDownload2.source.url);
      assert.equal(results[2].url, olderDownload1.source.url);
    });
    it("should format the description properly if there is no file type", async () => {
      newDownload.target.path = null;
      downloadsManager.onDownloadAdded(newDownload);
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
        onlySucceeded: true,
        onlyExists: true,
      });
      assert.equal(results.length, 1);
      assert.equal(results[0].description, "1.5 MB"); // see unit-entry.js to see where this comes from
    });
  });
  describe("#onDownloadRemoved", () => {
    let newDownload;
    beforeEach(() => {
      downloadsManager._downloadItems.clear();
      newDownload = {
        source: { url: "https://site.com/removeMe.mov" },
        endTime: Date.now(),
        target: { path: "/path/to/removeMe.mov", exists: true },
        succeeded: true,
        refresh: async () => {},
      };
      downloadsManager.onDownloadAdded(newDownload);
    });
    it("should remove a download if it exists on onDownloadRemoved", async () => {
      downloadsManager.onDownloadRemoved({
        source: { url: "https://site.com/removeMe.mov" },
      });
      const results = await downloadsManager.getDownloads(Infinity, {
        numItems: 5,
      });
      assert.deepEqual(results, []);
    });
    it("should dispatch DOWNLOAD_CHANGED when removing a download", () => {
      downloadsManager._store.dispatch = sinon.spy();
      downloadsManager.onDownloadRemoved({
        source: { url: "https://site.com/removeMe.mov" },
      });
      assert.calledOnce(downloadsManager._store.dispatch);
    });
  });
});
