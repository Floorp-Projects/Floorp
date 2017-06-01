const ACTIVITY_STREAM_PREF_BRANCH = "browser.newtabpage.activity-stream.";
const {Prefs, DefaultPrefs} = require("lib/ActivityStreamPrefs.jsm");

const TEST_PREF_CONFIG = [
  {name: "foo", value: true},
  {name: "bar", value: "BAR"},
  {name: "baz", value: 1}
];

describe("ActivityStreamPrefs", () => {
  describe("Prefs", () => {
    it("should have get, set, and observe methods", () => {
      const p = new Prefs();
      assert.property(p, "get");
      assert.property(p, "set");
      assert.property(p, "observe");
    });
    describe(".branchName", () => {
      it("should return the activity stream branch by default", () => {
        const p = new Prefs();
        assert.equal(p.branchName, ACTIVITY_STREAM_PREF_BRANCH);
      });
      it("should return the custom branch name if it was passed to the constructor", () => {
        const p = new Prefs("foo");
        assert.equal(p.branchName, "foo");
      });
    });
  });

  describe("DefaultPrefs", () => {
    describe("#init", () => {
      let defaultPrefs;
      beforeEach(() => {
        defaultPrefs = new DefaultPrefs(TEST_PREF_CONFIG);
        sinon.spy(defaultPrefs.branch, "setBoolPref");
        sinon.spy(defaultPrefs.branch, "setStringPref");
        sinon.spy(defaultPrefs.branch, "setIntPref");
      });
      it("should initialize a boolean pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.branch.setBoolPref, "foo", true);
      });
      it("should initialize a string pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.branch.setStringPref, "bar", "BAR");
      });
      it("should initialize a integer pref", () => {
        defaultPrefs.init();
        assert.calledWith(defaultPrefs.branch.setIntPref, "baz", 1);
      });
    });
    describe("#reset", () => {
      it("should clear user preferences for each pref in the config", () => {
        const defaultPrefs = new DefaultPrefs(TEST_PREF_CONFIG);
        sinon.spy(defaultPrefs.branch, "clearUserPref");
        defaultPrefs.reset();
        for (const pref of TEST_PREF_CONFIG) {
          assert.calledWith(defaultPrefs.branch.clearUserPref, pref.name);
        }
      });
    });
  });
});
