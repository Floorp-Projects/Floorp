/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "data:text/html;charset=UTF-8," +
  "<h1>browser_inspector_sidebarstate.js</h1>";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);

  info("Selecting ruleview.");
  inspector.sidebar.select("ruleview");

  is(inspector.sidebar.getCurrentTabID(), "ruleview",
     "Rule View is selected by default");

  info("Selecting computed view.");
  inspector.sidebar.select("computedview");

  // Finish initialization of the computed panel before
  // destroying the toolbox.
  yield waitForTick();

  info("Closing inspector.");
  yield toolbox.destroy();

  info("Re-opening inspector.");
  inspector = (yield openInspector()).inspector;

  if (!inspector.sidebar.getCurrentTabID()) {
    info("Default sidebar still to be selected, adding select listener.");
    yield inspector.sidebar.once("select");
  }

  is(inspector.sidebar.getCurrentTabID(), "computedview",
     "Computed view is selected by default.");
});
