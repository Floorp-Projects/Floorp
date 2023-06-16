/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test keyboard shortcuts.
 */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-debugger-statements.html");

  const onReloaded = reload(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-debugger-statements.html");
  const source = findSource(dbg, "doc-debugger-statements.html");
  assertPausedAtSourceAndLine(dbg, source.id, 11);

  await pressResume(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 16);

  await pressStepOver(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 17);

  await pressStepIn(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 22);

  await pressStepOut(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 18);

  await pressStepOver(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 18);

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;
});

function pressResume(dbg) {
  pressKey(dbg, "resumeKey");
  return waitForPaused(dbg);
}

function pressStepOver(dbg) {
  pressKey(dbg, "stepOverKey");
  return waitForPaused(dbg);
}

function pressStepIn(dbg) {
  pressKey(dbg, "stepInKey");
  return waitForPaused(dbg);
}

function pressStepOut(dbg) {
  pressKey(dbg, "stepOutKey");
  return waitForPaused(dbg);
}
