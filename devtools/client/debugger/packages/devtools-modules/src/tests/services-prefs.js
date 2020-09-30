const Services = require("devtools-services");
const pref = Services.pref;

describe("services prefs shim", () => {
  beforeEach(() => {
    // Add some starter prefs.
    localStorage.setItem("Services.prefs:devtools.branch1.somebool", JSON.stringify({
      // bool
      type: 128,
      defaultValue: false,
      hasUserValue: false,
      userValue: false
    }));

    localStorage.setItem("Services.prefs:devtools.branch1.somestring", JSON.stringify({
      // string
      type: 32,
      defaultValue: "dinosaurs",
      hasUserValue: true,
      userValue: "elephants"
    }));

    localStorage.setItem("Services.prefs:devtools.branch2.someint", JSON.stringify({
      // int
      type: 64,
      defaultValue: -16,
      hasUserValue: false,
      userValue: null
    }));
  });

  afterEach(() => {
    localStorage.clear();
  });

  it("can get and set preferences", () => {
    expect(Services.prefs.getBoolPref("devtools.branch1.somebool")).toBe(false);
    Services.prefs.setBoolPref("devtools.branch1.somebool", true);
    expect(Services.prefs.getBoolPref("devtools.branch1.somebool")).toBe(true);

    Services.prefs.clearUserPref("devtools.branch1.somestring");
    expect(Services.prefs.getCharPref("devtools.branch1.somestring")).toBe("dinosaurs");

    expect(Services.prefs.prefHasUserValue("devtools.branch1.somebool")).toBe(true,
      "bool pref has user value");
    expect(Services.prefs.prefHasUserValue("devtools.branch1.somestring")).toBe(false,
      "string pref does not have user value");

    // String prefs actually differ from Char prefs in the real implementation of
    // Services.prefs, but for this shim, both are using the same implementation.
    Services.prefs.setStringPref("devtools.branch1.somerealstring", "abcdef");
    expect(Services.prefs.getStringPref("devtools.branch1.somerealstring")).toBe("abcdef");
  });

  it("can call savePrefFile without crashing", () => {
    Services.prefs.savePrefFile(null);
  });

  it("can use branches", () => {
    let branch0 = Services.prefs.getBranch(null);
    let branch1 = Services.prefs.getBranch("devtools.branch1.");

    branch1.setCharPref("somestring", "octopus");
    Services.prefs.setCharPref("devtools.branch1.somestring", "octopus");
    expect(Services.prefs.getCharPref("devtools.branch1.somestring")).toEqual("octopus");
    expect(branch0.getCharPref("devtools.branch1.somestring")).toEqual("octopus");
    expect(branch1.getCharPref("somestring")).toEqual("octopus");
  });

  it("throws exceptions when expected", () => {
    expect(() => {
      Services.prefs.setIntPref("devtools.branch1.somebool", 27);
    }).toThrow();

    expect(() => {
      Services.prefs.setBoolPref("devtools.branch1.somebool", 27);
    }).toThrow();

    expect(() => {
      Services.prefs.getCharPref("devtools.branch2.someint");
    }).toThrow();

    expect(() => {
      Services.prefs.setCharPref("devtools.branch2.someint", "whatever");
    }).toThrow();

    expect(() => {
      Services.prefs.setIntPref("devtools.branch2.someint", "whatever");
    }).toThrow();

    expect(() => {
      Services.prefs.getBoolPref("devtools.branch1.somestring");
    }).toThrow();

    expect(() => {
      Services.prefs.setBoolPref("devtools.branch1.somestring", true);
    }).toThrow();

    expect(() => {
      Services.prefs.setCharPref("devtools.branch1.somestring", true);
    }).toThrow();
  });

  it("returns correct pref types", () => {
    expect(Services.prefs.getPrefType("devtools.branch1.somebool"))
        .toEqual(Services.prefs.PREF_BOOL);
    expect(Services.prefs.getPrefType("devtools.branch2.someint"))
        .toEqual(Services.prefs.PREF_INT);
    expect(Services.prefs.getPrefType("devtools.branch1.somestring"))
        .toEqual(Services.prefs.PREF_STRING);
  });

  it("supports observers", () => {
    let notifications = {};
    let clearNotificationList = () => {
      notifications = {};
    };

    let observer = {
      observe: function (subject, topic, data) {
        notifications[data] = true;
      }
    };

    let branch0 = Services.prefs.getBranch(null);
    let branch1 = Services.prefs.getBranch("devtools.branch1.");

    branch0.addObserver("devtools.branch1", null, null);
    branch0.addObserver("devtools.branch1.", observer);
    branch1.addObserver("", observer);

    Services.prefs.setCharPref("devtools.branch1.somestring", "elf owl");
    expect(notifications).toEqual({
      "devtools.branch1.somestring": true,
      "somestring": true
    }, "notifications sent to two listeners");

    clearNotificationList();
    Services.prefs.setIntPref("devtools.branch2.someint", 1729);
    expect(notifications).toEqual({}, "no notifications sent");

    clearNotificationList();
    branch0.removeObserver("devtools.branch1.", observer);
    Services.prefs.setCharPref("devtools.branch1.somestring", "tapir");
    expect(notifications).toEqual({
      "somestring": true
    }, "removeObserver worked");

    clearNotificationList();
    branch0.addObserver("devtools.branch1.somestring", observer);
    Services.prefs.setCharPref("devtools.branch1.somestring", "northern shoveler");
    expect(notifications).toEqual({
      "devtools.branch1.somestring": true,
      "somestring": true
    }, "notifications sent to two listeners");
    branch0.removeObserver("devtools.branch1.somestring", observer);

    // Make sure we update if the pref change comes from somewhere else.
    clearNotificationList();
    pref("devtools.branch1.someotherstring", "lazuli bunting");
    expect(notifications).toEqual({
      "someotherstring": true
    }, "pref worked");
  });

  it("does not crash when setting prefs (bug 1296427)", () => {
    // Regression test for bug 1296427.
    pref("devtools.hud.loglimit", 1000);
    pref("devtools.hud.loglimit.network", 1000);
  });

  it("fixes observer bug (bug 1319150)", () => {
    // Regression test for bug 1319150.
    let seen = false;
    let fnObserver = () => {
      seen = true;
    };

    let branch0 = Services.prefs.getBranch(null);
    branch0.addObserver("devtools.branch1.somestring", fnObserver);
    Services.prefs.setCharPref("devtools.branch1.somestring", "common merganser");
    expect(seen).toBe(true);
    branch0.removeObserver("devtools.branch1.somestring", fnObserver);
  });

  it("supports default value argument", () => {
    // Check support for default values
    let intPrefWithDefault =
        Services.prefs.getIntPref("devtools.branch1.missing", 1);
    expect(intPrefWithDefault).toEqual(1);

    let charPrefWithDefault =
        Services.prefs.getCharPref("devtools.branch1.missing", "test");
    expect(charPrefWithDefault).toEqual("test");

    let boolPrefWithDefault =
        Services.prefs.getBoolPref("devtools.branch1.missing", true);
    expect(boolPrefWithDefault).toBe(true);
  });
});
