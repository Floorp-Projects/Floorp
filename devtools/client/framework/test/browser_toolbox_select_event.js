/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL = "data:text/html;charset=utf-8,test select events";

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

  /**
   * Assert that selecting the given toolId raises a select event
   * @param {toolId} Id of the tool to test
   */
  function testSelectEvent(toolId) {
    return new Promise(resolve => {
      toolbox.once("select", (event, id) => {
        is(id, toolId, toolId + " selected");
        resolve();
      });
      toolbox.selectTool(toolId);
    });
  }

  /**
   * Assert that selecting the given toolId raises its corresponding
   * selected event
   * @param {toolId} Id of the tool to test
   */
  function testToolSelectEvent(toolId) {
    return new Promise(resolve => {
      toolbox.once(toolId + "-selected", () => {
        let msg = toolId + " tool selected";
        is(toolbox.currentToolId, toolId, msg);
        resolve();
      });
      toolbox.selectTool(toolId);
    });
  }
});

