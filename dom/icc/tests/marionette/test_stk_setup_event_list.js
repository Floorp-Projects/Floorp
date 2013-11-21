/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testSetupEventList(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, iccManager.STK_CMD_SET_UP_EVENT_LIST, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  for (let index in command.options.eventList) {
    is(command.options.eventList[index], expect.eventList[index], expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d00c810301050082028182990104",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_1",
            commandQualifier: 0x00,
            eventList: [4]}},
  {command: "d00d81030105008202818299020507",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_2",
            commandQualifier: 0x00,
            eventList: [5, 7]}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_3",
            commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_4",
            commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00b8103010500820281829900",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_5",
            commandQualifier: 0x00,
            eventList: null}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_6",
            commandQualifier: 0x00,
            eventList: [7]}}
];

runNextTest();
