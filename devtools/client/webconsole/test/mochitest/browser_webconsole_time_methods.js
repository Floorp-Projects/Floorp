/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Console API implements the time() and timeEnd() methods.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-time-methods.html";

const TEST_URI2 = "data:text/html;charset=utf-8,<script>" +
                  "console.timeEnd('bTimer');</script>";

const TEST_URI3 = "data:text/html;charset=utf-8,<script>" +
                  "console.time('bTimer');console.log('smoke signal');</script>";

const TEST_URI4 = "data:text/html;charset=utf-8," +
                  "<script>console.timeEnd('bTimer');</script>";

add_task(async function() {
  // Calling console.time('aTimer') followed by console.timeEnd('aTimer')
  // should result in the aTimer being ended, and a message like aTimer: 123ms
  // printed to the console
  const hud1 = await openNewTabAndConsole(TEST_URI);

  const aTimerCompleted = await waitFor(() => findMessage(hud1, "aTimer: "));
  ok(aTimerCompleted, "Calling console.time('a') and console.timeEnd('a')"
    + "ends the 'a' timer");

  // Calling console.time('bTimer') in the current tab, opening a new tab
  // and calling console.timeEnd('bTimer') in the new tab should not result in
  // the bTimer in the initial tab being ended, but rather a warning message
  // output to the console: Timer "bTimer" doesn't exist
  const hud2 = await openNewTabAndConsole(TEST_URI2);

  const error1 =
    await waitFor(() => findMessage(hud2, "bTimer", ".message.timeEnd.warn"));
  ok(error1, "Timers with the same name but in separate tabs do not contain "
    + "the same value");

  // The next tests make sure that timers with the same name but in separate
  // pages do not contain the same value.
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI3);

  // The new console front-end does not display a message when timers are started,
  // so there should not be a 'bTimer started' message on the output

  // We use this await to 'sync' until the message appears, as the console API
  // guarantees us that the smoke signal will be printed after the message for
  // console.time("bTimer") (if there were any)
  await waitFor(() => findMessage(hud2, "smoke signal"));

  is(findMessage(hud2, "bTimer started"), null, "No message is printed to "
    + "the console when the timer starts");

  hud2.ui.clearOutput();

  // Calling console.time('bTimer') on a page, then navigating to another page
  // and calling console.timeEnd('bTimer') on the new console front-end should
  // result on a warning message: 'Timer "bTimer" does not exist',
  // as the timers in different pages are not related
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI4);

  const error2 =
    await waitFor(() => findMessage(hud2, "bTimer", ".message.timeEnd.warn"));
  ok(error2, "Timers with the same name but in separate pages do not contain "
    + "the same value");
});
