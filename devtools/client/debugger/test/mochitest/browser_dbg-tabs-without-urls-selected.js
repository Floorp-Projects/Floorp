/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that URL-less sources have tabs and selecting that location does not
// create a new tab for the same URL-less source
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");

  // Create a URL-less source 
  const { hud } = await dbg.toolbox.selectTool("webconsole");
  await evaluateExpressionInConsole(hud, `
    (() => {
        setTimeout(() => { debugger; }, 100);
    })();
  `);
  await waitForPaused(dbg);

  // Click a frame which shouldn't open a new source tab
  await clickElement(dbg, "frame", 3);

  // Click the frame to select the same location and ensure there's only 1 tab
  is(countTabs(dbg), 1);

  await resume(dbg);
});
