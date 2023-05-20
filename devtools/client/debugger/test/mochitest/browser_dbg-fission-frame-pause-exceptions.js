/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const TEST_COM_URI = `${URL_ROOT_COM_SSL}examples/doc_dbg-fission-pause-exceptions.html`;
// Tests Pause on exceptions in remote iframes

add_task(async function () {
  // Load a test page with a remote iframe
  const dbg = await initDebuggerWithAbsoluteURL(TEST_COM_URI);

  info("Test pause on exceptions ignoring caught exceptions");
  await togglePauseOnExceptions(dbg, true, false);

  let onReloaded = reload(dbg);
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc_dbg-fission-frame-pause-exceptions.html").id,
    17
  );

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;

  info("Test pause on exceptions including caught exceptions");
  await togglePauseOnExceptions(dbg, true, true);

  onReloaded = reload(dbg);
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc_dbg-fission-frame-pause-exceptions.html").id,
    13
  );

  await resume(dbg);
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc_dbg-fission-frame-pause-exceptions.html").id,
    17
  );

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;
});
