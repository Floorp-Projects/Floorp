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

add_task(function*() {
  const dbg = yield initDebugger("doc-debugger-statements.html");

  yield reload(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg);

  yield pressResume(dbg);
  assertPausedLocation(dbg);

  yield pressStepIn(dbg);
  assertPausedLocation(dbg);

  yield pressStepOut(dbg);
  assertPausedLocation(dbg);

  yield pressStepOver(dbg);
  assertPausedLocation(dbg);
});
