import { DefaultPrefs, Prefs } from "lib/ActivityStreamPrefs.sys.mjs";

const TEST_PREF_CONFIG = new Map([
  ["foo", { value: true }],
  ["bar", { value: "BAR" }],
  ["baz", { value: 1 }],
  ["qux", { value: "foo", value_local_dev: "foofoo" }],
]);

describe("ActivityStreamPrefs", () => {
  describe("Prefs", () => {
    let p;
    beforeEach(() => {
      p = new Prefs();
    });
    it("should have get, set, and observe methods", () => {
      assert.property(p, "get");
      assert.property(p, "set");
      assert.property(p, "observe");
    });
    describe("#observeBranch", () => {
      let listener;
      beforeEach(() => {
        p._prefBranch = { addObserver: sinon.stub() };
        listener = { onPrefChanged: sinon.stub() };
        p.observeBranch(listener);
      });
      it("should add an observer", () => {
        assert.calledOnce(p._prefBranch.addObserver);
        assert.calledWith(p._prefBranch.addObserver, "");
      });
      it("should store the listener", () => {
        assert.equal(p._branchObservers.size, 1);
        assert.ok(p._branchObservers.has(listener));
      });
      it("should call listener's onPrefChanged", () => {
        p._branchObservers.get(listener)();

        assert.calledOnce(listener.onPrefChanged);
      });
    });
    describe("#ignoreBranch", () => {
      let listener;
      beforeEach(() => {
        p._prefBranch = {
          addObserver: sinon.stub(),
          removeObserver: sinon.stub(),
        };
        listener = {};
        p.observeBranch(listener);
      });
      it("should remove the observer", () => {
        p.ignoreBranch(listener);

        assert.calledOnce(p._prefBranch.removeObserver);
        assert.calledWith(
          p._prefBranch.removeObserver,
          p._prefBranch.addObserver.firstCall.args[0]
        );
      });
      it("should remove the listener", () => {
        assert.equal(p._branchObservers.size, 1);

        p.ignoreBranch(listener);

        assert.equal(p._branchObservers.size, 0);
      });
    });
  });

  describe("DefaultPrefs", () => {
    describe("#init", () => {
      let defaultPrefs;
      let sandbox;
      beforeEach(() => {
        sandbox = sinon.createSandbox();
        defaultPrefs = new DefaultPrefs(TEST_PREF_CONFIG);
        sinon.stub(defaultPrefs, "set");
      });
      afterEach(() => {
        sandbox.restore();
      });
      it("should initialize a boolean pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.set, "foo", true);
      });
      it("should not initialize a pref if a default exists", () => {
        defaultPrefs.prefs.set("foo", false);

        defaultPrefs.init();

        assert.neverCalledWith(defaultPrefs.set, "foo", true);
      });
      it("should initialize a string pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.set, "bar", "BAR");
      });
      it("should initialize a integer pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.set, "baz", 1);
      });
      it("should initialize a pref with value if Firefox is not a local build", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.set, "qux", "foo");
      });
      it("should initialize a pref with value_local_dev if Firefox is a local build", () => {
        sandbox.stub(global.AppConstants, "MOZILLA_OFFICIAL").value(false);
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.set, "qux", "foofoo");
      });
    });
  });
});
