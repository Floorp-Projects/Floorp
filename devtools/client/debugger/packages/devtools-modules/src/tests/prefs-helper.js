const { PrefsHelper } = require("../prefs");
const Services = require("devtools-services");
const pref = Services.pref;

describe("prefs helper", () => {
  beforeEach(() => {});

  afterEach(() => {
    localStorage.clear();
  });

  it("supports fallback values", () => {
    pref("devtools.valid", "{\"valid\": true}");
    pref("devtools.invalid", "{not valid at all]");
    pref("devtools.nodefault", "{not valid at all]");
    pref("devtools.validnodefault", "{\"nodefault\": true}");

    const prefs = new PrefsHelper("devtools", {
      valid: ["Json", "valid", {valid: true}],
      invalid: ["Json", "invalid", {}],
      nodefault: ["Json", "nodefault"],
      validnodefault: ["Json", "validnodefault"],
    });

    // Valid Json pref should return the actual value
    expect(prefs.valid).toEqual({valid: true});

    // Invalid Json pref with a fallback shoud return the fallback value
    expect(prefs.invalid).toEqual({});

    // Invalid Json pref with no fallback should throw
    expect(() => prefs.nodefault).toThrow();

    // Valid Json pref with no fallback should return the value
    expect(prefs.validnodefault).toEqual({nodefault: true});
  });
});
