/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getGrids for the LayoutActor

const GRID_FRAGMENT_DATA = {
  areas: [
    {
      columnEnd: 3,
      columnStart: 2,
      name: "header",
      rowEnd: 2,
      rowStart: 1,
      type: "explicit"
    },
    {
      columnEnd: 2,
      columnStart: 1,
      name: "sidebar",
      rowEnd: 3,
      rowStart: 2,
      type: "explicit"
    },
    {
      columnEnd: 3,
      columnStart: 2,
      name: "content",
      rowEnd: 3,
      rowStart: 2,
      type: "explicit"
    }
  ],
  cols: {
    lines: [
      {
        breadth: 0,
        names: ["col-1", "col-start-1", "sidebar-start"],
        number: 1,
        start: 0
      },
      {
        breadth: 0,
        names: ["col-2", "header-start", "sidebar-end", "content-start"],
        number: 2,
        start: 100
      },
      {
        breadth: 0,
        names: ["header-end", "content-end"],
        number: 3,
        start: 200
      }
    ],
    tracks: [
      {
        breadth: 100,
        start: 0,
        state: "static",
        type: "explicit"
      },
      {
        breadth: 100,
        start: 100,
        state: "static",
        type: "explicit"
      }
    ]
  },
  rows: {
    lines: [
      {
        breadth: 0,
        names: ["header-start"],
        number: 1,
        start: 0
      },
      {
        breadth: 0,
        names: ["header-end", "sidebar-start", "content-start"],
        number: 2,
        start: 100
      },
      {
        breadth: 0,
        names: ["sidebar-end", "content-end"],
        number: 3,
        start: 200
      }
    ],
    tracks: [
      {
        breadth: 100,
        start: 0,
        state: "static",
        type: "explicit"
      },
      {
        breadth: 100,
        start: 100,
        state: "static",
        type: "explicit"
      }
    ]
  }
};

add_task(async function() {
  const { client, walker, layout } =
    await initLayoutFrontForUrl(MAIN_DOMAIN + "grid.html");
  const grids = await layout.getGrids(walker.rootNode);
  const grid = grids[0];
  const { gridFragments } = grid;

  is(grids.length, 1, "One grid was returned.");
  is(gridFragments.length, 1, "One grid fragment was returned.");
  ok(Array.isArray(gridFragments), "An array of grid fragments was returned.");
  Assert.deepEqual(gridFragments[0], GRID_FRAGMENT_DATA,
    "Got the correct grid fragment data.");

  info("Get the grid container node front.");

  try {
    const nodeFront = await walker.getNodeFromActor(grids[0].actorID, ["containerEl"]);
    ok(nodeFront, "Got the grid container node front.");
  } catch (e) {
    ok(false, "Did not get grid container node front.");
  }

  await client.close();
  gBrowser.removeCurrentTab();
});
