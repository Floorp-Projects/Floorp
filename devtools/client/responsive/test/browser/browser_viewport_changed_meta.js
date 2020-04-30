/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that resolution is as expected when the viewport tag is changed.
// The page content is a 400 x 400 div in a 200 x 200 viewport. Initially,
// the viewport width is set to 800 at initial-scale 1, but then the tag
// content is changed. This triggers various rescale operations that will
// changge the resolution of the page after reflow.

// Chrome handles many of these cases differently. The Chrome results are
// included as TODOs, but labelled as "res_chrome" to indicate that the
// goal is not necessarily to match an agreed-upon standard, but to
// achieve web compatability through changing either Firefox or Chrome
// behavior.

info("--- Starting viewport test output ---");

const WIDTH = 200;
const HEIGHT = 200;
const INITIAL_CONTENT = "width=800, initial-scale=1";
const INITIAL_RES_TARGET = 1.0;
const TESTS = [
  // This checks that when the replaced content matches the original content,
  // we get the same values as the original values.
  { content: INITIAL_CONTENT, res_target: INITIAL_RES_TARGET },

  // Section 1: Check the case of a viewport shrinking with the display width
  // staying the same. In this case, the shrink will fit the max of the 400px
  // content width and the viewport width into the 200px display area.
  { content: "width=200", res_target: 0.5 }, // fitting 400px content
  { content: "width=400", res_target: 0.5 }, // fitting 400px content/viewport
  { content: "width=500", res_target: 0.4 }, // fitting 500px viewport

  // Section 2: Same as Section 1, but adds user-scalable=no. The expected
  // results are similar to Section 1, but we ignore the content size and only
  // adjust resolution to make the viewport fit into the display area.
  { content: "width=200, user-scalable=no", res_target: 1.0 },
  { content: "width=400, user-scalable=no", res_target: 0.5 },
  { content: "width=500, user-scalable=no", res_target: 0.4 },

  // Section 3: Same as Section 1, but adds initial-scale=1. Initial-scale
  // prevents content shrink in Firefox, so the viewport is scaled based on its
  // changing size relative to the display area. In this case, the resolution
  // is increased to maintain the proportional amount of the previously visible
  // content. With the initial conditions, the display area was showing 1/4 of
  // the content at 0.25x resolution. As the viewport width is shrunk, the
  // resolution will increase to ensure that only 1/4 of the content is visible.
  // Essentially, the new viewport width times the resolution will equal 800px,
  // the original viewport width times resolution.
  //
  // Chrome treats the initial-scale=1 as inviolable and sets resolution to 1.0.
  { content: "width=200, initial-scale=1", res_target: 4.0, res_chrome: 1.0 },
  { content: "width=400, initial-scale=1", res_target: 2.0, res_chrome: 1.0 },
  { content: "width=500, initial-scale=1", res_target: 1.6, res_chrome: 1.0 },

  // Section 4: Same as Section 3, but adds user-scalable=no. The combination
  // of this and initial-scale=1 prevents the scaling-up of the resolution to
  // keep the proportional amount of the previously visible content.
  { content: "width=200, initial-scale=1, user-scalable=no", res_target: 1.0 },
  { content: "width=400, initial-scale=1, user-scalable=no", res_target: 1.0 },
  { content: "width=500, initial-scale=1, user-scalable=no", res_target: 1.0 },
];

const TEST_URL =
  `data:text/html;charset=utf-8,` +
  `<html><head><meta name="viewport" content="${INITIAL_CONTENT}"></head>` +
  `<body style="margin:0">` +
  `<div id="box" style="width:400px;height:400px;background-color:green">` +
  `Initial</div></body></html>`;

addRDMTask(
  TEST_URL,
  async function({ ui, manager, browser }) {
    await setViewportSize(ui, manager, WIDTH, HEIGHT);
    await setTouchAndMetaViewportSupport(ui, true);

    // Check initial resolution value.
    const initial_resolution = await spawnViewportTask(ui, {}, () => {
      return content.windowUtils.getResolution();
    });

    is(
      initial_resolution.toFixed(2),
      INITIAL_RES_TARGET.toFixed(2),
      `Initial resolution is as expected.`
    );

    for (const test of TESTS) {
      const { content: content, res_target, res_chrome } = test;

      await spawnViewportTask(ui, { content }, args => {
        const box = content.document.getElementById("box");
        box.textContent = args.content;

        const meta = content.document.getElementsByTagName("meta")[0];
        info(`Changing meta viewport content to "${args.content}".`);
        meta.content = args.content;
      });

      await promiseContentReflow(ui);

      const resolution = await spawnViewportTask(ui, {}, () => {
        return content.windowUtils.getResolution();
      });

      is(
        resolution.toFixed(2),
        res_target.toFixed(2),
        `Replaced meta viewport content "${content}" resolution is as expected.`
      );

      if (typeof res_chrome !== "undefined") {
        todo_is(
          resolution.toFixed(2),
          res_chrome.toFixed(2),
          `Replaced meta viewport content "${content}" resolution matches Chrome resolution.`
        );
      }

      // Reload to prepare for next test.
      const reload = waitForViewportLoad(ui);
      browser.reload();
      await reload;
    }
  },
  { usingBrowserUI: true }
);
