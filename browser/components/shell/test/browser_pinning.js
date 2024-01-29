/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_getTaskbarTabShortcutPath() {
  // no exception generated for the following
  ShellService.getTaskbarTabShortcutPath("working");
  ShellService.getTaskbarTabShortcutPath("workingPath.ink");

  // confirm that multiple spaces aren't collapsed
  ShellService.getTaskbarTabShortcutPath("working    path.ink");

  const invalidFilenames = [
    // These are reserved characters that can't be in names
    "?",
    ":",
    "<",
    ">",
    "|",
    '"',
    "/",
    "*",
    "\t",
    "\r",
    "\n",

    // No path manipulation allowed
    "..\\something",
    ".\\something",
    ".something",

    // Windows doesn't allow filenames ending in period or a space
    "something.",
    "something ",

    // The following are special reserved names
    "CON",
    "PRN",
    "AUX",
    "NUL",
    "COM1",
    "COM2",
    "COM3",
    "COM4",
    "COM5",
    "COM6",
    "COM7",
    "COM8",
    "COM9",
    "LPT1",
    "LPT2",
    "LPT3",
    "LPT4",
    "LPT5",
    "LPT6",
    "LPT7",
    "LPT8",
    "LPT9",
  ];

  for (const invalidFilename of invalidFilenames) {
    Assert.throws(
      () => {
        ShellService.getTaskbarTabShortcutPath(invalidFilename);
      },
      /NS_ERROR_FILE_INVALID_PATH/,
      invalidFilename +
        " is an invalid filename; getTaskbarTabShortcutPath should have failed with it as a parameter."
    );
  }
});
