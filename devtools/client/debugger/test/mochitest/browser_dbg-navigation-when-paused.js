/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-navigation-when-paused.html");

  await togglePauseOnExceptions(dbg, true, true);

  clickElementInTab("body");

  await waitForPaused(dbg, "doc-navigation-when-paused.html");

  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-navigation-when-paused.html").id,
    12
  );

  await navigate(
    dbg,
    "doc-navigation-when-paused.html",
    "doc-navigation-when-paused.html"
  );

  clickElementInTab("body");

  await waitForPaused(dbg, "doc-navigation-when-paused.html");

  // If breakpoints aren't properly ignored after navigation, this could
  // potentially pause at line 9. This helper also ensures that the file
  // source itself has loaded, which may not be the case if navigation cleared
  // the source and nothing has sent it to the devtools client yet, as was
  // the case in Bug 1581530.
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-navigation-when-paused.html").id,
    12
  );
});
