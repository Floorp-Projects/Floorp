import {ActivityStreamStorage} from "lib/ActivityStreamStorage.jsm";
import {GlobalOverrider} from "test/unit/utils";

let overrider = new GlobalOverrider();

describe("ActivityStreamStorage", () => {
  let sandbox;
  let indexedDB;
  let storage;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    indexedDB = {open: sandbox.stub().returns(Promise.resolve({}))};
    overrider.set({IndexedDB: indexedDB});
    storage = new ActivityStreamStorage("storage_test");
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should not throw an error when accessing db", async () => {
    assert.ok(storage.db);
  });
  it("should revert key value parameters for put", async () => {
    const stub = sandbox.stub();
    sandbox.stub(storage, "getStore").resolves({put: stub});

    await storage.set("key", "value");

    assert.calledOnce(stub);
    assert.calledWith(stub, "value", "key");
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
    storage = new ActivityStreamStorage(["store1", "store2"]);
    const dbStub = {createObjectStore: sandbox.stub(), objectStoreNames: {contains: sandbox.stub().returns(false)}};
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledTwice(dbStub.createObjectStore);
    assert.calledWith(dbStub.createObjectStore, "store1");
    assert.calledWith(dbStub.createObjectStore, "store2");
  });
  it("should skip creating existing stores", async () => {
    storage = new ActivityStreamStorage(["store1", "store2"]);
    const dbStub = {createObjectStore: sandbox.stub(), objectStoreNames: {contains: sandbox.stub().returns(true)}};
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.notCalled(dbStub.createObjectStore);
  });
});
