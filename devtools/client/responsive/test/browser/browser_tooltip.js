/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CONTENT = `<h1 title="test title" style="margin: 5px">test h1</h1>`;
const TEST_URL = `data:text/html;charset=utf-8,${TEST_CONTENT}`;

// Test for the tooltip coordinate on the browsing document in RDM.

addRDMTask(TEST_URL, async ({ ui }) => {
  // Make this synthesize mouse events via the parent process because some
  // code may cache the mouse cursor position like PresShell.  That may prevent
  // unexpected DOM events coming from the parent process.
  await pushPref("test.events.async.enabled", true);

  info("Create a promise which waits until the tooltip will be shown");
  const tooltip = ui.browserWindow.gBrowser.ownerDocument.getElementById(
    "remoteBrowserTooltip"
  );
  const onTooltipShown = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  const onTooltipHidden = BrowserTestUtils.waitForEvent(tooltip, "popuphidden");

  info("Show a tooltip");
  await spawnViewportTask(ui, {}, async () => {
    const target = content.document.querySelector("h1");

    // First, make sure to move mouse cursor outside the target.
    info("Waiting for initial mousemove");
    {
      // FYI: Some `mousemove` events may not be sent to the content process
      // until all preparations done in the main process and the content
      // process. Therefore, we need to synthesize `mousemove`s until we
      // get it in the remote process at least once.  Therefore, we cannot
      // use a promise to wait a `mousemove` here and we need to use the while
      // loop below.
      let mouseMoveFired = false;
      content.document.addEventListener(
        "mousemove",
        event => {
          isnot(
            event.target,
            target,
            "The first mousemove should be fired outside the target"
          );
          mouseMoveFired = true;
        },
        {
          once: true,
        }
      );
      while (!mouseMoveFired) {
        await EventUtils.synthesizeMouse(
          target,
          -2,
          -2,
          { type: "mousemove" },
          content
        );
        await EventUtils.synthesizeMouse(
          target,
          -1,
          -1,
          { type: "mousemove" },
          content
        );
        // Wait for the tooltip for the target is hidden even if it was visible.
        await new Promise(resolve =>
          content.window.requestAnimationFrame(() =>
            content.window.requestAnimationFrame(resolve)
          )
        );
      }
    }

    const eventLogger = event =>
      info(
        `${event.type}: path=[${event.composedPath()}], outerHTML=${
          event.target.outerHTML
        }, relatedTarget=${event.relatedTarget?.outerHTML}`
      );
    target.addEventListener("mouseover", eventLogger);
    target.addEventListener("mouseout", eventLogger);
    content.document.addEventListener("mousemove", eventLogger);
    // Then, move cursor over the target.
    await EventUtils.synthesizeMouse(
      target,
      1,
      1,
      { type: "mousemove", isSynthesized: false },
      content
    );
    await EventUtils.synthesizeMouse(
      target,
      2,
      1,
      { type: "mousemove", isSynthesized: false },
      content
    );
    await EventUtils.synthesizeMouse(
      target,
      3,
      1,
      { type: "mousemove", isSynthesized: false },
      content
    );
  });

  info("Wait for showing the tooltip");
  await onTooltipShown;

  info("Test the X coordinate of the tooltip");
  isnot(tooltip.screenX, 0, "The X coordinate of tooltip should not be 0");

  // Finally, clean up the tooltip if we're running in the verify mode.
  info("Wait for hiding the tooltip");
  await spawnViewportTask(ui, {}, async () => {
    info("Cleaning up the tooltip with moving the cursor");
    const target = content.document.querySelector("h1");
    await EventUtils.synthesizeMouse(
      target,
      -1,
      -1,
      { type: "mousemove", isSynthesized: false },
      content
    );
  });
  await onTooltipHidden;
});
