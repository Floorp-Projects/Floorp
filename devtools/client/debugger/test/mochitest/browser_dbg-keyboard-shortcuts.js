/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test keyboard shortcuts.
 */

add_task(async function() {
  const dbg = await initDebugger("doc-debugger-statements.html");

  await reload(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-debugger-statements.html");
  assertPausedLocation(dbg, "doc-debugger-statements");

  await pressResume(dbg);
  assertPausedLocation(dbg);

  await pressStepOver(dbg);
  assertPausedLocation(dbg);

  await pressStepIn(dbg);
  assertPausedLocation(dbg);

  await pressStepOut(dbg);
  assertPausedLocation(dbg);

  await pressStepOver(dbg);
  assertPausedLocation(dbg);
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
