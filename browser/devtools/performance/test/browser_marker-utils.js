/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the marker utils methods.
 */

function* spawnTest() {
  let { TIMELINE_BLUEPRINT } = devtools.require("devtools/performance/markers");
  let Utils = devtools.require("devtools/performance/marker-utils");

  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);

  is(Utils.getMarkerLabel({ name: "DOMEvent" }), "DOM Event",
    "getMarkerLabel() returns a simple label");
  is(Utils.getMarkerLabel({ name: "Javascript", causeName: "setTimeout handler" }), "setTimeout",
    "getMarkerLabel() returns a label defined via function");

  ok(Utils.getMarkerFields({ name: "Paint" }).length === 0,
    "getMarkerFields() returns an empty array when no fields defined");

  let fields = Utils.getMarkerFields({ name: "ConsoleTime", causeName: "snowstorm" });
  is(fields[0].label, "Timer Name:", "getMarkerFields() returns an array with proper label");
  is(fields[0].value, "snowstorm", "getMarkerFields() returns an array with proper value");

  fields = Utils.getMarkerFields({ name: "DOMEvent", type: "mouseclick" });
  is(fields.length, 1, "getMarkerFields() ignores fields that are not found on marker");
  is(fields[0].label, "Event Type:", "getMarkerFields() returns an array with proper label");
  is(fields[0].value, "mouseclick", "getMarkerFields() returns an array with proper value");

  fields = Utils.getMarkerFields({ name: "DOMEvent", eventPhase: Ci.nsIDOMEvent.AT_TARGET, type: "mouseclick" });
  is(fields.length, 2, "getMarkerFields() returns multiple fields when using a fields function");
  is(fields[0].label, "Event Type:", "getMarkerFields() correctly returns fields via function (1)");
  is(fields[0].value, "mouseclick", "getMarkerFields() correctly returns fields via function (2)");
  is(fields[1].label, "Phase:", "getMarkerFields() correctly returns fields via function (3)");
  is(fields[1].value, "Target", "getMarkerFields() correctly returns fields via function (4)");

  is(Utils.getMarkerFields({ name: "Javascript", causeName: "Some Platform Field" })[0].value, "(Gecko)",
    "Correctly obfuscates JS markers when platform data is off.");
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);
  is(Utils.getMarkerFields({ name: "Javascript", causeName: "Some Platform Field" })[0].value, "Some Platform Field",
    "Correctly deobfuscates JS markers when platform data is on.");

  is(Utils.getMarkerClassName("Javascript"), "Function Call",
    "getMarkerClassName() returns correct string when defined via function");
  is(Utils.getMarkerClassName("GarbageCollection"), "GC Event",
    "getMarkerClassName() returns correct string when defined via function");
  is(Utils.getMarkerClassName("Reflow"), "Layout",
    "getMarkerClassName() returns correct string when defined via string");

  TIMELINE_BLUEPRINT["fakemarker"] = { group: 0 };
  try {
    Utils.getMarkerClassName("fakemarker");
    ok(false, "getMarkerClassName() should throw when no label on blueprint.");
  } catch (e) {
    ok(true, "getMarkerClassName() should throw when no label on blueprint.");
  }

  TIMELINE_BLUEPRINT["fakemarker"] = { group: 0, label: () => void 0};
  try {
    Utils.getMarkerClassName("fakemarker");
    ok(false, "getMarkerClassName() should throw when label function returnd undefined.");
  } catch (e) {
    ok(true, "getMarkerClassName() should throw when label function returnd undefined.");
  }

  delete TIMELINE_BLUEPRINT["fakemarker"];

  finish();
}
