/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const testServer = createVersionizedHttpTestServer("sourcemaps-reload2");
const TEST_URL = testServer.urlFor("index.html");

// Assert the behavior of the gutter that grays out non-breakable lines
add_task(async function testBreakableLinesOverReloads() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL, "index.html", "script.js", "original.js");

  info("Assert breakable lines of the first html page load");
  await assertBreakableLines(dbg, "index.html", 20, [[16], [17]]);

  info("Assert breakable lines of the first original source file, original.js");
  // The length of original.js is longer than the test file
  // because the sourcemap replaces the content of the original file
  // and appends a few lines with a "WEBPACK FOOTER" comment
  // All the appended lines are empty lines or comments, so none of them are breakable.
  await assertBreakableLines(dbg, "original.js", 13, [[1,3], [5,8]]);

  info("Assert breakable lines of the simple first load of script.js");
  await assertBreakableLines(dbg, "script.js", 3, [[1], [3]]);

  info("Reload the page, wait for sources and assert that breakable lines get updated");
  testServer.switchToNextVersion();
  await reload(dbg, "index.html", "script.js", "original.js");

  info("Assert breakable lines of the more complex second load of script.js");
  await assertBreakableLines(dbg, "script.js", 23, [[2], [13,23]]);

  info("Assert breakable lines of the second html page load");
  await assertBreakableLines(dbg, "index.html", 21, [[15], [17]]);

  info("Assert breakable lines of the second orignal file");
  // See first assertion about original.js,
  // the size of original.js doesn't match the size of the test file
  await assertBreakableLines(dbg, "original.js", 18, [[1,3], [8,11], [13]]);
});

function assertLineIsBreakable(dbg, file, line, shouldBeBreakable) {
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  // When a line is not breakable, the "empty-line" class is added
  // and the line is greyed out
  if (shouldBeBreakable) {
    ok(
      !lineInfo.wrapClass?.includes("empty-line"),
      `${file}:${line} should be breakable`);
  } else {
    ok(
      lineInfo?.wrapClass?.includes("empty-line"),
      `${file}:${line} should NOT be breakable`);
  }
}

function shouldLineBeBreakable(breakableLines, line) {
  for(const range of breakableLines) {
    if (range.length == 2) {
      if (line >= range[0] && line <= range[1]) {
        return true;
      }
    } else if (range.length == 1) {
      if (line == range[0]) {
        return true;
      }
    } else {
      ok(false, "Ranges of breakable lines should only be made of arrays, with one item for single lines, or two items for subsequent breakable lines");
    }
  }
  return false;
}

async function assertBreakableLines(dbg, file, numberOfLines, breakableLines) {
  await selectSource(dbg, file);
  const editorLines = dbg.win.document.querySelectorAll(".CodeMirror-lines .CodeMirror-code > div");
  is(editorLines.length, numberOfLines, `We show the expected number of lines in CodeMirror for ${file}`);
  for(let line = 1; line <= numberOfLines; line++) {
    assertLineIsBreakable(dbg, file, line, shouldLineBeBreakable(breakableLines, line));
  }
}
