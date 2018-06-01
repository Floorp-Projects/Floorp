/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple checks for the LayoutActor and GridActor

add_task(async function() {
  const {client, walker, layout} = await initLayoutFrontForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>");

  ok(layout, "The LayoutFront was created");
  ok(layout.getGrids, "The getGrids method exists");

  let didThrow = false;
  try {
    await layout.getGrids(null);
  } catch (e) {
    didThrow = true;
  }
  ok(didThrow, "An exception was thrown for a missing NodeActor in getGrids");

  const invalidNode = await walker.querySelector(walker.rootNode, "title");
  const grids = await layout.getGrids(invalidNode);
  ok(Array.isArray(grids), "An array of grids was returned");
  is(grids.length, 0, "0 grids have been returned for the invalid node");

  await client.close();
  gBrowser.removeCurrentTab();
});
