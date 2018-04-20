import {ActivityStreamStorage} from "lib/ActivityStreamStorage.jsm";
import {GlobalOverrider} from "test/unit/utils";

let overrider = new GlobalOverrider();

describe("ActivityStreamStorage", () => {
  let sandbox;
  let indexedDB;
  let storage;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    indexedDB = {open: sandbox.stub().resolves({})};
    overrider.set({IndexedDB: indexedDB});
    storage = new ActivityStreamStorage({
      storeNames: ["storage_test"],
      telemetry: {handleUndesiredEvent: sandbox.stub()}
    });
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should not throw an error when accessing db", async () => {
    assert.ok(storage.db);
  });
  it("should throw if arguments not provided", () => {
    assert.throws(() => new ActivityStreamStorage());
  });
  describe("#getDbTable", () => {
    let testStorage;
    let storeStub;
    beforeEach(() => {
      storeStub = {
        getAll: sandbox.stub().resolves(),
        get: sandbox.stub().resolves(),
        put: sandbox.stub().resolves()
      };
      sandbox.stub(storage, "_getStore").resolves(storeStub);
      testStorage = storage.getDbTable("storage_test");
    });
    it("should reverse key value parameters for put", async () => {
      await testStorage.set("key", "value");

      assert.calledOnce(storeStub.put);
      assert.calledWith(storeStub.put, "value", "key");
    });
    it("should return the correct value for get", async () => {
      storeStub.get.withArgs("foo").resolves("foo");

      const result = await testStorage.get("foo");

      assert.calledOnce(storeStub.get);
      assert.equal(result, "foo");
    });
    it("should return the correct value for getAll", async () => {
      storeStub.getAll.resolves(["bar"]);

      const result = await testStorage.getAll();

      assert.calledOnce(storeStub.getAll);
      assert.deepEqual(result, ["bar"]);
    });
    it("should query the correct object store", async () => {
      await testStorage.get();

      assert.calledOnce(storage._getStore);
      assert.calledWithExactly(storage._getStore, "storage_test");
    });
    it("should throw if table is not found", () => {
      assert.throws(() => storage.getDbTable("undefined_store"));
    });
  });
  it("should get the correct objectStore when calling _getStore", async () => {
    const objectStoreStub = sandbox.stub();
    indexedDB.open.resolves({objectStore: objectStoreStub});

    await storage._getStore("foo");

    assert.calledOnce(objectStoreStub);
    assert.calledWithExactly(objectStoreStub, "foo", "readwrite");
  });
  it("should create a db with the correct store name", async () => {
    const dbStub = {createObjectStore: sandbox.stub(), objectStoreNames: {contains: sandbox.stub().returns(false)}};
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledOnce(dbStub.createObjectStore);
    assert.calledWithExactly(dbStub.createObjectStore, "storage_test");
  });
  it("should handle an array of object store names", async () => {
    storage = new ActivityStreamStorage({
      storeNames: ["store1", "store2"],
      telemetry: {}
    });
    const dbStub = {createObjectStore: sandbox.stub(), objectStoreNames: {contains: sandbox.stub().returns(false)}};
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledTwice(dbStub.createObjectStore);
    assert.calledWith(dbStub.createObjectStore, "store1");
    assert.calledWith(dbStub.createObjectStore, "store2");
  });
  it("should skip creating existing stores", async () => {
    storage = new ActivityStreamStorage({
      storeNames: ["store1", "store2"],
      telemetry: {}
    });
    const dbStub = {createObjectStore: sandbox.stub(), objectStoreNames: {contains: sandbox.stub().returns(true)}};
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.notCalled(dbStub.createObjectStore);
  });
  describe("#_requestWrapper", () => {
    it("should return a successful result", async () => {
      const result = await storage._requestWrapper(() => Promise.resolve("foo"));

      assert.equal(result, "foo");
      assert.notCalled(storage.telemetry.handleUndesiredEvent);
    });
    it("should report failures", async () => {
      try {
        await storage._requestWrapper(() => Promise.reject(new Error()));
      } catch (e) {
        assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      }
    });
  });
});
