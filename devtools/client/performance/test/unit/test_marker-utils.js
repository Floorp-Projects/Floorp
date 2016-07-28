/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests the marker utils methods.
 */

function run_test() {
  run_next_test();
}

add_task(function () {
  let { TIMELINE_BLUEPRINT } = require("devtools/client/performance/modules/markers");
  let { PREFS } = require("devtools/client/performance/modules/global");
  let { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");

  PREFS.registerObserver();

  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);

  equal(MarkerBlueprintUtils.getMarkerLabel(
    { name: "DOMEvent" }), "DOM Event",
    "getMarkerLabel() returns a simple label");
  equal(MarkerBlueprintUtils.getMarkerLabel(
    { name: "Javascript", causeName: "setTimeout handler" }), "setTimeout",
    "getMarkerLabel() returns a label defined via function");
  equal(MarkerBlueprintUtils.getMarkerLabel(
    { name: "GarbageCollection", causeName: "ALLOC_TRIGGER" }), "Incremental GC",
    "getMarkerLabel() returns a label for a function that is generalizable");

  ok(MarkerBlueprintUtils.getMarkerFields({ name: "Paint" }).length === 0,
    "getMarkerFields() returns an empty array when no fields defined");

  let fields = MarkerBlueprintUtils.getMarkerFields(
    { name: "ConsoleTime", causeName: "snowstorm" });
  equal(fields[0].label, "Timer Name:",
        "getMarkerFields() returns an array with proper label");
  equal(fields[0].value, "snowstorm",
        "getMarkerFields() returns an array with proper value");

  fields = MarkerBlueprintUtils.getMarkerFields({ name: "DOMEvent", type: "mouseclick" });
  equal(fields.length, 1,
        "getMarkerFields() ignores fields that are not found on marker");
  equal(fields[0].label, "Event Type:",
        "getMarkerFields() returns an array with proper label");
  equal(fields[0].value, "mouseclick",
        "getMarkerFields() returns an array with proper value");

  fields = MarkerBlueprintUtils.getMarkerFields(
    { name: "DOMEvent", eventPhase: Ci.nsIDOMEvent.AT_TARGET, type: "mouseclick" });
  equal(fields.length, 2,
        "getMarkerFields() returns multiple fields when using a fields function");
  equal(fields[0].label, "Event Type:",
        "getMarkerFields() correctly returns fields via function (1)");
  equal(fields[0].value, "mouseclick",
        "getMarkerFields() correctly returns fields via function (2)");
  equal(fields[1].label, "Phase:",
        "getMarkerFields() correctly returns fields via function (3)");
  equal(fields[1].value, "Target",
        "getMarkerFields() correctly returns fields via function (4)");

  fields = MarkerBlueprintUtils.getMarkerFields(
    { name: "GarbageCollection", causeName: "ALLOC_TRIGGER" });
  equal(fields[0].value, "Too Many Allocations", "Uses L10N for GC reasons");

  fields = MarkerBlueprintUtils.getMarkerFields(
    { name: "GarbageCollection", causeName: "NOT_A_GC_REASON" });
  equal(fields[0].value, "NOT_A_GC_REASON",
        "Defaults to enum for GC reasons when not L10N'd");

  equal(MarkerBlueprintUtils.getMarkerFields(
    { name: "Javascript", causeName: "Some Platform Field" })[0].value, "(Gecko)",
    "Correctly obfuscates JS markers when platform data is off.");
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);
  equal(MarkerBlueprintUtils.getMarkerFields(
    { name: "Javascript", causeName: "Some Platform Field" })[0].value,
    "Some Platform Field",
    "Correctly deobfuscates JS markers when platform data is on.");

  equal(MarkerBlueprintUtils.getMarkerGenericName("Javascript"), "Function Call",
    "getMarkerGenericName() returns correct string when defined via function");
  equal(MarkerBlueprintUtils.getMarkerGenericName("GarbageCollection"),
    "Garbage Collection",
    "getMarkerGenericName() returns correct string when defined via function");
  equal(MarkerBlueprintUtils.getMarkerGenericName("Reflow"), "Layout",
    "getMarkerGenericName() returns correct string when defined via string");

  TIMELINE_BLUEPRINT.fakemarker = { group: 0 };
  try {
    MarkerBlueprintUtils.getMarkerGenericName("fakemarker");
    ok(false, "getMarkerGenericName() should throw when no label on blueprint.");
  } catch (e) {
    ok(true, "getMarkerGenericName() should throw when no label on blueprint.");
  }

  TIMELINE_BLUEPRINT.fakemarker = { group: 0, label: () => void 0 };
  try {
    MarkerBlueprintUtils.getMarkerGenericName("fakemarker");
    ok(false,
       "getMarkerGenericName() should throw when label function returnd undefined.");
  } catch (e) {
    ok(true,
       "getMarkerGenericName() should throw when label function returnd undefined.");
  }

  delete TIMELINE_BLUEPRINT.fakemarker;

  equal(MarkerBlueprintUtils.getBlueprintFor({ name: "Reflow" }).label, "Layout",
    "getBlueprintFor() should return marker def for passed in marker.");
  equal(MarkerBlueprintUtils.getBlueprintFor({ name: "Not sure!" }).label(), "Unknown",
    "getBlueprintFor() should return a default marker def if the marker is undefined.");

  PREFS.unregisterObserver();
});
