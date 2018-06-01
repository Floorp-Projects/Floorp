/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1377094 - Test that all column headers have tooltips.
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document } = monitor.panelWin;

  const headers = document.querySelectorAll(".requests-list-header-button");
  for (const header of headers) {
    const buttonText = header.querySelector(".button-text").textContent;
    const tooltip = header.getAttribute("title");
    is(buttonText, tooltip,
       "The " + header.id + " header has the button text in its 'title' attribute.");
  }

  await teardown(monitor);
});
