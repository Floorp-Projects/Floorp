#!/usr/bin/env node

/* eslint-env node */

var gift = require("gift"),
  fs = require("fs"),
  argv = require("optimist")
    .usage(
      "Update vtt.jsm with the latest from a vtt.js directory.\nUsage:" +
        " $0 -d [dir]"
    )
    .demand("d")
    .options("d", {
      alias: "dir",
      describe: "Path to WebVTT directory.",
    })
    .options("r", {
      alias: "rev",
      describe: "Revision to update to.",
      default: "master",
    })
    .options("w", {
      alias: "write",
      describe: "Path to file to write to.",
      default: "./vtt.jsm",
    }).argv;

var repo = gift(argv.d);
repo.status(function(err, status) {
  if (!status.clean) {
    console.log("The repository's working directory is not clean. Aborting.");
    process.exit(1);
  }
  repo.checkout(argv.r, function() {
    repo.commits(argv.r, 1, function(err, commits) {
      var vttjs = fs.readFileSync(argv.d + "/lib/vtt.js", "utf8");

      // Remove settings for VIM and Emacs.
      vttjs = vttjs.replace(/\/\* -\*-.*-\*- \*\/\n/, "");
      vttjs = vttjs.replace(/\/\* vim:.* \*\/\n/, "");

      // Concatenate header and vttjs code.
      vttjs =
        "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
        " * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
        " * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n" +
        'this.EXPORTED_SYMBOLS = ["WebVTT"];\n\n' +
        "/**\n" +
        " * Code below is vtt.js the JS WebVTT implementation.\n" +
        " * Current source code can be found at http://github.com/mozilla/vtt.js\n" +
        " *\n" +
        " * Code taken from commit " +
        commits[0].id +
        "\n" +
        " */\n" +
        vttjs;

      fs.writeFileSync(argv.w, vttjs);
    });
  });
});
