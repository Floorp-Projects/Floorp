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
  it("should throw an error if you try to use it without init", () => {
    assert.throws(() => storage.db);
  });
  it("should not throw an error if you called init", async () => {
    await storage.init();
    assert.ok(storage.db);
  });
  it("should revert key value parameters for put", () => {
    const stub = sandbox.stub();
    sandbox.stub(storage, "getStore").returns({put: stub});

    storage.set("key", "value");

    assert.calledOnce(stub);
    assert.calledWith(stub, "value", "key");
  });
  it("should create a db with the correct store name", async () => {
    const dbStub = {createObjectStore: sandbox.stub()};
    await storage.init();

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledOnce(dbStub.createObjectStore);
    assert.calledWithExactly(dbStub.createObjectStore, "storage_test");
  });
});
