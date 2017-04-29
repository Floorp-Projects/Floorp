/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple checks for the LayoutActor and GridActor

add_task(function* () {
  let {client, walker, layout} = yield initLayoutFrontForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>");

  ok(layout, "The LayoutFront was created");
  ok(layout.getAllGrids, "The getAllGrids method exists");

  let didThrow = false;
  try {
    yield layout.getGrids(null);
  } catch (e) {
    didThrow = true;
  }
  ok(didThrow, "An exception was thrown for a missing NodeActor in getGrids");

  didThrow = false;
  try {
    yield layout.getAllGrids(null);
  } catch (e) {
    didThrow = true;
  }
  ok(didThrow, "An exception was thrown for a missing NodeActor in getAllGrids");

  let invalidNode = yield walker.querySelector(walker.rootNode, "title");
  let grids = yield layout.getAllGrids(invalidNode, true);
  ok(Array.isArray(grids), "An array of grids was returned");
  is(grids.length, 0, "0 grids have been returned for the invalid node");

  yield client.close();
  gBrowser.removeCurrentTab();
});
