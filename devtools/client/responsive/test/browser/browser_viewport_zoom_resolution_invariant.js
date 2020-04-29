/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that resolution is as expected for different types of meta viewport
// settings, as the RDM pane is zoomed to different values.

const RESOLUTION_FACTOR_MIN = 0.96;
const RESOLUTION_FACTOR_MAX = 1.04;
const ZOOM_LEVELS = [
  0.3,
  0.5,
  0.67,
  0.8,
  0.9,
  1.0,
  1.1,
  1.2,
  1.33,
  1.5,
  1.7,
  2.0,
  2.4,
  3.0,
  // TODO(emilio): These should pass.
  // 0.3,
  // 3.0,
];

info("--- Starting viewport test output ---");

const WIDTH = 200;
const HEIGHT = 200;
const TESTS = [
  { content: "width=600", res_target: 0.333 },
  { content: "width=600, initial-scale=1.0", res_target: 1.0 },
  { content: "width=device-width", res_target: 1.0 },
  { content: "width=device-width, initial-scale=2.0", res_target: 2.0 },
];

for (const { content, res_target } of TESTS) {
  const TEST_URL =
    `data:text/html;charset=utf-8,` +
    `<html><head><meta name="viewport" content="${content}"></head>` +
    `<body><div style="width:100%;background-color:green">${content}</div>` +
    `</body></html>`;

  addRDMTask(
    TEST_URL,
    async function({ ui, manager, browser, usingBrowserUI }) {
      info(
        `Using meta viewport content "${content}" with new RDM UI ${usingBrowserUI}.`
      );

      await setViewportSize(ui, manager, WIDTH, HEIGHT);
      await setTouchAndMetaViewportSupport(ui, true);

      // Ensure we've reflowed the page at least once so that MVM has chosen
      // the initial scale.
      await promiseContentReflow(ui);

      for (const zoom of ZOOM_LEVELS.concat([...ZOOM_LEVELS].reverse())) {
        info(`Set zoom to ${zoom}.`);
        await promiseRDMZoom(ui, browser, zoom);

        const resolution = await spawnViewportTask(ui, {}, () => {
          return content.windowUtils.getResolution();
        });

        const res_min = res_target * RESOLUTION_FACTOR_MIN;
        const res_max = res_target * RESOLUTION_FACTOR_MAX;
        ok(
          res_min <= resolution && res_max >= resolution,
          `${content} zoom ${zoom} resolution should be near ${res_target}, and we got ${resolution}.`
        );
      }
    },
    { usingBrowserUI: true }
  );
}
