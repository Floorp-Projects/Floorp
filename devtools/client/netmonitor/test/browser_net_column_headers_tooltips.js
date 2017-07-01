/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1377094 - Test that all column headers have tooltips.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document } = monitor.panelWin;

  let headers = document.querySelectorAll(".requests-list-header-button");
  for (let header of headers) {
    const buttonText = header.querySelector(".button-text").textContent;
    const tooltip = header.getAttribute("title");
    is(buttonText, tooltip,
       "The " + header.id + " header has the button text in its 'title' attribute.");
  }

  yield teardown(monitor);
});
