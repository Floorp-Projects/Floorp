/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testPollOff(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, iccManager.STK_CMD_POLL_OFF, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);

  runNextTest();
}

let tests = [
  {command: "d009810301040082028182",
   func: testPollOff,
   expect: {name: "pull_off_cmd_1",
            commandQualifier: 0x00}}
];

runNextTest();
