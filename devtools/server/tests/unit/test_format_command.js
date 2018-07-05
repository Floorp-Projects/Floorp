/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

const { formatCommand } = require("devtools/server/actors/webconsole/commands.js");

const testcases = [
  { input: ":help", expectedOutput: "help()" },
  {
    input: ":screenshot  --fullscreen",
    expectedOutput: "screenshot({\"fullscreen\":true})"
  },
  {
    input: ":screenshot  --fullscreen true",
    expectedOutput: "screenshot({\"fullscreen\":true})"
  },
  { input: ":screenshot  ", expectedOutput: "screenshot()" },
  {
    input: ":screenshot --dpr 0.5 --fullpage --chrome",
    expectedOutput: "screenshot({\"dpr\":0.5,\"fullpage\":true,\"chrome\":true})"
  },
  {
    input: ":screenshot 'filename'",
    expectedOutput: "screenshot({\"filename\":\"filename\"})"
  },
  {
    input: ":screenshot filename",
    expectedOutput: "screenshot({\"filename\":\"filename\"})"
  },
  {
    input: ":screenshot --name 'filename' --name `filename` --name \"filename\"",
    expectedOutput: "screenshot({\"name\":[\"filename\",\"filename\",\"filename\"]})"
  },
  {
    input: ":screenshot 'filename1' 'filename2' 'filename3'",
    expectedOutput: "screenshot({\"filename\":\"filename1\"})"
  },
  {
    input: ":screenshot --chrome --chrome",
    expectedOutput: "screenshot({\"chrome\":true})"
  },
  {
    input: ":screenshot \"file name with spaces\"",
    expectedOutput: "screenshot({\"filename\":\"file name with spaces\"})"
  },
  {
    input: ":screenshot 'filename1' --name 'filename2'",
    expectedOutput: "screenshot({\"filename\":\"filename1\",\"name\":\"filename2\"})"
  },
  {
    input: ":screenshot --name 'filename1' 'filename2'",
    expectedOutput: "screenshot({\"name\":\"filename1\",\"filename\":\"filename2\"})"
  },
  {
    input: ":screenshot \"fo\\\"o bar\"",
    expectedOutput: "screenshot({\"filename\":\"fo\\\\\\\"o bar\"})"
  },
  {
    input: ":screenshot \"foo b\\\"ar\"",
    expectedOutput: "screenshot({\"filename\":\"foo b\\\\\\\"ar\"})"
  }
];

const edgecases = [
  { input: ":", expectedError: /'' is not a valid command/ },
  { input: ":invalid", expectedError: /'invalid' is not a valid command/ },
  { input: ":screenshot :help", expectedError: /Invalid command/ },
  { input: ":screenshot --", expectedError: /invalid flag/ },
  {
    input: ":screenshot \"fo\"o bar",
    // XXX Bug 1473569 - this should be: /String contains unexpected `\"` character/
    expectedError: /String does not terminate/
  },
  {
    input: ":screenshot \"foo b\"ar",
    // XXX Bug 1473569 - this should be: /String contains unexpected `\"` character/
    expectedError: /String does not terminate/
  },
  { input: ": screenshot", expectedError: /'' is not a valid command/ },
  { input: ":screenshot \"file name", expectedError: /String does not terminate/ },
  {
    input: ":screenshot \"file name --clipboard",
    // XXX Bug 1473569 - this should be:
    // /String does not terminate before flag \"clipboard\"/
    expectedError: /String does not terminate before flag clipboard/
  },
  { input: "::screenshot", expectedError: /':screenshot' is not a valid command/ }
];

function run_test() {
  testcases.forEach(testcase => {
    Assert.equal(formatCommand(testcase.input), testcase.expectedOutput);
  });

  edgecases.forEach(testcase => {
    Assert.throws(() => formatCommand(testcase.input), testcase.expectedError,
      `"${testcase.input}" should throw expected error`);
  });
}
