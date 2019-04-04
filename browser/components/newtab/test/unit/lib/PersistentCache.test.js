import {GlobalOverrider} from "test/unit/utils";
import {PersistentCache} from "lib/PersistentCache.jsm";

describe("PersistentCache", () => {
  let fakeOS;
  let fakeJsonParse;
  let fakeFetch;
  let cache;
  let filename = "cache.json";
  let reportErrorStub;
  let globals;
  let sandbox;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    fakeOS = {
      Constants: {Path: {localProfileDir: "/foo/bar"}},
      File: {
        writeAtomic: sinon.stub().returns(Promise.resolve()),
      },
      Path: {join: () => filename},
    };
    fakeJsonParse = sandbox.stub().resolves({});
    fakeFetch = sandbox.stub().resolves({json: fakeJsonParse});
    reportErrorStub = sandbox.stub();
    globals.set("OS", fakeOS);
    globals.set("Cu", {reportError: reportErrorStub});
    globals.set("fetch", fakeFetch);

    cache = new PersistentCache(filename);
  });
  afterEach(() => {
    globals.restore();
    sandbox.restore();
  });

  describe("#get", () => {
    it("tries to fetch the file", async () => {
      await cache.get("foo");
      assert.calledOnce(fakeFetch);
    });
    it("doesnt try to parse file if it doesn't exist", async () => {
      fakeFetch.throws();
      await cache.get("foo");
      assert.notCalled(fakeJsonParse);
    });
    it("doesnt try to fetch the file if it was already loaded", async () => {
      await cache._load();
      fakeFetch.resetHistory();
      await cache.get("foo");
      assert.notCalled(fakeFetch);
    });
    it("should catch and report errors", async () => {
      fakeJsonParse.throws();
      await cache._load();
      assert.calledOnce(reportErrorStub);
    });
    it("returns data for a given cache key", async () => {
      fakeJsonParse.resolves({foo: "bar"});
      let value = await cache.get("foo");
      assert.equal(value, "bar");
    });
    it("returns undefined for a cache key that doesn't exist", async () => {
      let value = await cache.get("baz");
      assert.equal(value, undefined);
    });
    it("returns all the data if no cache key is specified", async () => {
      fakeJsonParse.resolves({foo: "bar"});
      let value = await cache.get();
      assert.deepEqual(value, {foo: "bar"});
    });
  });

  describe("#set", () => {
    it("tries to fetch the file on the first set", async () => {
      await cache.set("foo", {x: 42});
      assert.calledOnce(fakeFetch);
    });
    it("doesnt try to fetch the file if it was already loaded", async () => {
      cache = new PersistentCache(filename, true);
      await cache._load();
      fakeFetch.resetHistory();
      await cache.set("foo", {x: 42});
      assert.notCalled(fakeFetch);
    });
    it("tries to fetch the file on the first set", async () => {
      await cache.set("foo", {x: 42});
      assert.calledOnce(fakeFetch);
    });
    it("sets a string value", async () => {
      const key = "testkey";
      const value = "testvalue";
      await cache.set(key, value);
      const cachedValue = await cache.get(key);
      assert.equal(cachedValue, value);
    });
    it("sets an object value", async () => {
      const key = "testkey";
      const value = {x: 1, y: 2, z: 3};
      await cache.set(key, value);
      const cachedValue = await cache.get(key);
      assert.deepEqual(cachedValue, value);
    });
    it("writes the data to file", async () => {
      const key = "testkey";
      const value = {x: 1, y: 2, z: 3};
      fakeOS.File.exists = async () => false;
      await cache.set(key, value);
      assert.calledOnce(fakeOS.File.writeAtomic);
      assert.calledWith(fakeOS.File.writeAtomic, filename, `{"testkey":{"x":1,"y":2,"z":3}}`, {tmpPath: `${filename}.tmp`});
    });
  });
});
