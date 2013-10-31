/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testRefresh(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, iccManager.STK_CMD_REFRESH, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);

  runNextTest();
}

let tests = [
  {command: "d0108103010101820281829205013f002fe2",
   func: testRefresh,
   expect: {name: "refresh_cmd_1",
            commandQualifier: 0x01}},
  {command: "d009810301010482028182",
   func: testRefresh,
   expect: {name: "refresh_cmd_2",
            commandQualifier: 0x04}}
];

runNextTest();
