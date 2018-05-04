/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "data:text/html;charset=UTF-8," +
  "<h1>browser_inspector_sidebarstate.js</h1>";

add_task(async function() {
  let { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  info("Selecting computed view.");
  inspector.sidebar.select("computedview");

  is(inspector.sidebar.getCurrentTabID(), "computedview",
    "Computed View is selected");

  info("Selecting layout view.");
  inspector.sidebar.select("layoutview");

  is(inspector.sidebar.getCurrentTabID(), "layoutview",
    "Layout View is selected");

  info("Closing inspector.");
  await toolbox.destroy();

  info("Re-opening inspector.");
  inspector = (await openInspector()).inspector;

  if (!inspector.sidebar.getCurrentTabID()) {
    info("Default sidebar still to be selected, adding select listener.");
    await inspector.sidebar.once("select");
  }

  is(inspector.sidebar.getCurrentTabID(), "layoutview",
     "Layout view is selected by default.");
});
