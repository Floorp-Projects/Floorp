/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test keyboard shortcuts.
 */

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
