/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL = "data:text/html;charset=utf-8,test select events";

requestLongerTimeout(2);

add_task(function* () {
  let tab = yield addTab(PAGE_URL);

  let toolbox = yield openToolboxForTab(tab, "webconsole", "bottom");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");

  yield testToolSelectEvent("inspector");
  yield testToolSelectEvent("webconsole");
  yield testToolSelectEvent("styleeditor");
  yield toolbox.destroy();

  toolbox = yield openToolboxForTab(tab, "webconsole", "side");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");
  yield toolbox.destroy();

  toolbox = yield openToolboxForTab(tab, "webconsole", "window");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");
  yield testSelectEvent("inspector");
  yield testSelectEvent("webconsole");
  yield testSelectEvent("styleeditor");
  yield toolbox.destroy();

  yield testSelectToolRace();

  /**
   * Assert that selecting the given toolId raises a select event
   * @param {toolId} Id of the tool to test
   */
  function* testSelectEvent(toolId) {
    let onSelect = toolbox.once("select");
    toolbox.selectTool(toolId);
    let id = yield onSelect;
    is(id, toolId, toolId + " selected");
  }

  /**
   * Assert that selecting the given toolId raises its corresponding
   * selected event
   * @param {toolId} Id of the tool to test
   */
  function* testToolSelectEvent(toolId) {
    let onSelected = toolbox.once(toolId + "-selected");
    toolbox.selectTool(toolId);
    yield onSelected;
    is(toolbox.currentToolId, toolId, toolId + " tool selected");
  }

  /**
   * Assert that two calls to selectTool won't race
   */
  function* testSelectToolRace() {
    let toolbox = yield openToolboxForTab(tab, "webconsole");
    let selected = false;
    let onSelect = (event, id) => {
      if (selected) {
        ok(false, "Got more than one 'select' event");
      } else {
        selected = true;
      }
    };
    toolbox.once("select", onSelect);
    let p1 = toolbox.selectTool("inspector")
    let p2 = toolbox.selectTool("inspector");
    // Check that both promises don't resolve too early
    let checkSelectToolResolution = panel => {
      ok(selected, "selectTool resolves only after 'select' event is fired");
      let inspector = toolbox.getPanel("inspector");
      is(panel, inspector, "selecTool resolves to the panel instance");
    };
    p1.then(checkSelectToolResolution);
    p2.then(checkSelectToolResolution);
    yield p1;
    yield p2;

    yield toolbox.destroy();
  }
});

