import { GlobalOverrider } from "test/unit/utils";
import { PersistentCache } from "lib/PersistentCache.sys.mjs";

describe("PersistentCache", () => {
  let fakeIOUtils;
  let fakePathUtils;
  let cache;
  let filename = "cache.json";
  let consoleErrorStub;
  let globals;
  let sandbox;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    fakeIOUtils = {
      writeJSON: sinon.stub().resolves(0),
      readJSON: sinon.stub().resolves({}),
    };
    fakePathUtils = {
      join: sinon.stub().returns(filename),
      localProfileDir: "/",
    };
    consoleErrorStub = sandbox.stub();
    globals.set("console", { error: consoleErrorStub });
    globals.set("IOUtils", fakeIOUtils);
    globals.set("PathUtils", fakePathUtils);

    cache = new PersistentCache(filename);
  });
  afterEach(() => {
    globals.restore();
    sandbox.restore();
  });

  describe("#get", () => {
    it("tries to read the file", async () => {
      await cache.get("foo");
      assert.calledOnce(fakeIOUtils.readJSON);
    });
    it("doesnt try to read the file if it was already loaded", async () => {
      await cache._load();
      fakeIOUtils.readJSON.resetHistory();
      await cache.get("foo");
      assert.notCalled(fakeIOUtils.readJSON);
    });
    it("should catch and report errors", async () => {
      fakeIOUtils.readJSON.rejects(new SyntaxError("Failed to parse JSON"));
      await cache._load();
      assert.calledOnce(consoleErrorStub);

      cache._cache = undefined;
      consoleErrorStub.resetHistory();

      fakeIOUtils.readJSON.rejects(
        new DOMException("IOUtils shutting down", "AbortError")
      );
      await cache._load();
      assert.calledOnce(consoleErrorStub);

      cache._cache = undefined;
      consoleErrorStub.resetHistory();

      fakeIOUtils.readJSON.rejects(
        new DOMException("File not found", "NotFoundError")
      );
      await cache._load();
      assert.notCalled(consoleErrorStub);
    });
    it("returns data for a given cache key", async () => {
      fakeIOUtils.readJSON.resolves({ foo: "bar" });
      let value = await cache.get("foo");
      assert.equal(value, "bar");
    });
    it("returns undefined for a cache key that doesn't exist", async () => {
      let value = await cache.get("baz");
      assert.equal(value, undefined);
    });
    it("returns all the data if no cache key is specified", async () => {
      fakeIOUtils.readJSON.resolves({ foo: "bar" });
      let value = await cache.get();
      assert.deepEqual(value, { foo: "bar" });
    });
  });

  describe("#set", () => {
    it("tries to read the file on the first set", async () => {
      await cache.set("foo", { x: 42 });
      assert.calledOnce(fakeIOUtils.readJSON);
    });
    it("doesnt try to read the file if it was already loaded", async () => {
      cache = new PersistentCache(filename, true);
      await cache._load();
      fakeIOUtils.readJSON.resetHistory();
      await cache.set("foo", { x: 42 });
      assert.notCalled(fakeIOUtils.readJSON);
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
      const value = { x: 1, y: 2, z: 3 };
      await cache.set(key, value);
      const cachedValue = await cache.get(key);
      assert.deepEqual(cachedValue, value);
    });
    it("writes the data to file", async () => {
      const key = "testkey";
      const value = { x: 1, y: 2, z: 3 };

      await cache.set(key, value);
      assert.calledOnce(fakeIOUtils.writeJSON);
      assert.calledWith(
        fakeIOUtils.writeJSON,
        filename,
        { [[key]]: value },
        { tmpPath: `${filename}.tmp` }
      );
    });
    it("throws when failing to get file path", async () => {
      Object.defineProperty(fakePathUtils, "localProfileDir", {
        get() {
          throw new Error();
        },
      });

      let rejected = false;
      try {
        await cache.set("key", "val");
      } catch (error) {
        rejected = true;
      }

      assert(rejected);
    });
  });
});
