/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests sourcemaps.

add_task(function* () {
  const dbg = yield initDebugger("doc-sourcemaps.html");

  yield waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  ok(true, "Original sources exist");

  yield selectSource(dbg, "output.js");
  ok(dbg.win.cm.getValue().includes("function output"),
     "Original source text loaded correctly");
});
