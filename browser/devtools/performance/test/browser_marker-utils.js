/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the marker utils methods.
 */

function spawnTest () {
  let { TIMELINE_BLUEPRINT } = devtools.require("devtools/shared/timeline/global");
  let Utils = devtools.require("devtools/shared/timeline/marker-utils");

  is(Utils.getMarkerLabel({ name: "DOMEvent" }), "DOM Event",
    "getMarkerLabel() returns a simple label");
  is(Utils.getMarkerLabel({ name: "Javascript", causeName: "js" }), "js",
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
  is(fields.length, 2, "getMarkerFields() returns multiple fields when they exist");
  is(fields[0].label, "Event Type:", "getMarkerFields() returns an array with proper label (ordered)");
  is(fields[0].value, "mouseclick", "getMarkerFields() returns an array with proper value (ordered)");
  is(fields[1].label, "Phase:", "getMarkerFields() returns an array with proper label (ordered)");
  is(fields[1].value, "Target", "getMarkerFields() uses the `formatter` function when available");

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
