"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const {
  setDefaultPreferencesIfNeeded,
  PREFERENCE_TYPES,
} = require("resource://devtools/client/aboutdebugging/src/modules/runtime-default-preferences.js");

const CHAR_PREF = "some.char.pref";
const BOOL_PREF = "some.bool.pref";
const INT_PREF = "some.int.pref";

const TEST_PREFERENCES = [
  {
    prefName: BOOL_PREF,
    defaultValue: false,
    trait: "boolPrefTrait",
    type: PREFERENCE_TYPES.BOOL,
  },
  {
    prefName: CHAR_PREF,
    defaultValue: "",
    trait: "charPrefTrait",
    type: PREFERENCE_TYPES.CHAR,
  },
  {
    prefName: INT_PREF,
    defaultValue: 1,
    trait: "intPrefTrait",
    type: PREFERENCE_TYPES.INT,
  },
];

add_task(async function test_with_traits() {
  // Create a front that indicates that the preferences should be safe to query.
  // We should not perform any additional call to the preferences front.
  const preferencesFront = {
    getTraits: () => ({
      boolPrefTrait: true,
      charPrefTrait: true,
      intPrefTrait: true,
    }),

    setBoolPref: sinon.spy(),
    getBoolPref: sinon.spy(),
    setCharPref: sinon.spy(),
    getCharPref: sinon.spy(),
    setIntPref: sinon.spy(),
    getIntPref: sinon.spy(),
  };

  const clientWrapper = createClientWrapper(preferencesFront);
  await setDefaultPreferencesIfNeeded(clientWrapper, TEST_PREFERENCES);

  // Check get/setBoolPref spies
  ok(preferencesFront.getBoolPref.notCalled, "getBoolPref was not called");
  ok(preferencesFront.setBoolPref.notCalled, "setBoolPref was not called");

  // Check get/setCharPref spies
  ok(preferencesFront.getCharPref.notCalled, "getCharPref was not called");
  ok(preferencesFront.setCharPref.notCalled, "setCharPref was not called");

  // Check get/setIntPref spies
  ok(preferencesFront.getIntPref.notCalled, "getIntPref was not called");
  ok(preferencesFront.setIntPref.notCalled, "setIntPref was not called");
});

add_task(async function test_without_traits_no_error() {
  // Create a front that indicates that the preferences are missing, but which
  // doesn't fail when getting the preferences. This will typically happen when
  // the user managed to set the preference on the remote runtime.
  // We should not erase user values, so we should not call the set*Pref APIs.
  const preferencesFront = {
    getTraits: () => ({
      boolPrefTrait: false,
      charPrefTrait: false,
      intPrefTrait: false,
    }),

    setBoolPref: sinon.spy(),
    getBoolPref: sinon.spy(),
    setCharPref: sinon.spy(),
    getCharPref: sinon.spy(),
    setIntPref: sinon.spy(),
    getIntPref: sinon.spy(),
  };

  const clientWrapper = createClientWrapper(preferencesFront);
  await setDefaultPreferencesIfNeeded(clientWrapper, TEST_PREFERENCES);

  // Check get/setBoolPref spies
  ok(
    preferencesFront.getBoolPref.calledWith(BOOL_PREF),
    "getBoolPref was called with the proper preference name"
  );
  ok(preferencesFront.getBoolPref.calledOnce, "getBoolPref was called once");
  ok(preferencesFront.setBoolPref.notCalled, "setBoolPref was not called");

  // Check get/setCharPref spies
  ok(
    preferencesFront.getCharPref.calledWith(CHAR_PREF),
    "getCharPref was called with the proper preference name"
  );
  ok(preferencesFront.getCharPref.calledOnce, "getCharPref was called once");
  ok(preferencesFront.setCharPref.notCalled, "setCharPref was not called");

  // Check get/setIntPref spies
  ok(
    preferencesFront.getIntPref.calledWith(INT_PREF),
    "getIntPref was called with the proper preference name"
  );
  ok(preferencesFront.getIntPref.calledOnce, "getIntPref was called once");
  ok(preferencesFront.setIntPref.notCalled, "setIntPref was not called");
});

add_task(async function test_without_traits_with_error() {
  // Create a front that indicates that the preferences are missing, and which
  // will also throw when attempting to get said preferences.
  // This should lead to create default values for the preferences.
  const preferencesFront = {
    getTraits: () => ({
      boolPrefTrait: false,
      charPrefTrait: false,
      intPrefTrait: false,
    }),

    setBoolPref: sinon.spy(),
    getBoolPref: sinon.spy(pref => {
      if (pref === BOOL_PREF) {
        throw new Error("Invalid preference");
      }
    }),
    setCharPref: sinon.spy(),
    getCharPref: sinon.spy(pref => {
      if (pref === CHAR_PREF) {
        throw new Error("Invalid preference");
      }
    }),
    setIntPref: sinon.spy(),
    getIntPref: sinon.spy(pref => {
      if (pref === INT_PREF) {
        throw new Error("Invalid preference");
      }
    }),
  };

  const clientWrapper = createClientWrapper(preferencesFront);
  await setDefaultPreferencesIfNeeded(clientWrapper, TEST_PREFERENCES);

  // Check get/setBoolPref spies
  ok(preferencesFront.getBoolPref.calledOnce, "getBoolPref was called once");
  ok(preferencesFront.getBoolPref.threw(), "getBoolPref threw");
  ok(
    preferencesFront.getBoolPref.calledWith(BOOL_PREF),
    "getBoolPref was called with the proper preference name"
  );

  ok(preferencesFront.setBoolPref.calledOnce, "setBoolPref was called once");
  ok(
    preferencesFront.setBoolPref.calledWith(BOOL_PREF, false),
    "setBoolPref was called with the proper preference name and value"
  );

  // Check get/setCharPref spies
  ok(preferencesFront.getCharPref.calledOnce, "getCharPref was called once");
  ok(preferencesFront.getCharPref.threw(), "getCharPref threw");
  ok(
    preferencesFront.getCharPref.calledWith(CHAR_PREF),
    "getCharPref was called with the proper preference name"
  );

  ok(preferencesFront.setCharPref.calledOnce, "setCharPref was called once");
  ok(
    preferencesFront.setCharPref.calledWith(CHAR_PREF, ""),
    "setCharPref was called with the proper preference name and value"
  );

  // Check get/setIntPref spies
  ok(preferencesFront.getIntPref.calledOnce, "getIntPref was called once");
  ok(preferencesFront.getIntPref.threw(), "getIntPref threw");
  ok(
    preferencesFront.getIntPref.calledWith(INT_PREF),
    "getIntPref was called with the proper preference name"
  );

  ok(preferencesFront.setIntPref.calledOnce, "setIntPref was called once");
  ok(
    preferencesFront.setIntPref.calledWith(INT_PREF, 1),
    "setIntPref was called with the proper preference name and value"
  );
});

function createClientWrapper(preferencesFront) {
  const clientWrapper = {
    getFront: name => {
      return preferencesFront;
    },
  };

  return clientWrapper;
}
