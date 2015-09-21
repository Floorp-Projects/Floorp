/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that ViewHelpers.Prefs work properly with custom types of Float and Json.

var {ViewHelpers} = Cu.import("resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm", {});

function test() {
  let originalJson = Services.prefs.getCharPref("devtools.performance.timeline.hidden-markers");
  let originalFloat = Services.prefs.getCharPref("devtools.performance.memory.sample-probability");

  let Prefs = new ViewHelpers.Prefs("devtools.performance", {
    "float": ["Float", "memory.sample-probability"],
    "json": ["Json", "timeline.hidden-markers"]
  });

  Prefs.registerObserver();

  // Float
  Services.prefs.setCharPref("devtools.performance.timeline.hidden-markers", "{\"a\":1}");
  is(Prefs.json.a, 1, "The JSON pref value is correctly casted on get.");

  Prefs.json = { b: 2 };
  is(Prefs.json.a, undefined, "The JSON pref value is correctly casted on set (1).");
  is(Prefs.json.b, 2, "The JSON pref value is correctly casted on set (2).");

  // Float
  Services.prefs.setCharPref("devtools.performance.memory.sample-probability", "3.14");
  is(Prefs.float, 3.14, "The float pref value is correctly casted on get.");

  Prefs.float = 6.28;
  is(Prefs.float, 6.28, "The float pref value is correctly casted on set.");

  Prefs.unregisterObserver();

  Services.prefs.setCharPref("devtools.performance.timeline.hidden-markers", originalJson);
  Services.prefs.setCharPref("devtools.performance.memory.sample-probability", originalFloat);
  finish();
}
