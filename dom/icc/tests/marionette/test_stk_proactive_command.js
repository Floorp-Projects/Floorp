/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testLocalInfoLocation(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_LOCAL_INFO_LOCATION_INFO);
  is(cmd.options.localInfoType, iccManager.STK_LOCAL_INFO_LOCATION_INFO);

  runNextTest();
}

function testLocalInfoImei(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_LOCAL_INFO_IMEI);
  is(cmd.options.localInfoType, iccManager.STK_LOCAL_INFO_IMEI);

  runNextTest();
}

function testLocalInfoDate(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_LOCAL_INFO_DATE_TIME_ZONE);
  is(cmd.options.localInfoType, iccManager.STK_LOCAL_INFO_DATE_TIME_ZONE);

  runNextTest();
}

function testLocalInfoLanguage(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_LOCAL_INFO_LANGUAGE);
  is(cmd.options.localInfoType, iccManager.STK_LOCAL_INFO_LANGUAGE);

  runNextTest();
}

function testRefresh(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_REFRESH);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, 0x01);
  is(cmd.options, null);

  runNextTest();
}

function testTimerManagementStart(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_TIMER_MANAGEMENT);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_TIMER_START);
  is(cmd.options.timerAction, iccManager.STK_TIMER_START);
  is(cmd.options.timerId, 0x01);
  is(cmd.options.timerValue, (0x01 * 60 * 60) + (0x02 * 60) + 0x03);

  runNextTest();
}

function testTimerManagementDeactivate(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_TIMER_MANAGEMENT);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_TIMER_DEACTIVATE);
  is(cmd.options.timerAction, iccManager.STK_TIMER_DEACTIVATE);
  is(cmd.options.timerId, 0x04);

  runNextTest();
}

function testTimerManagementGetCurrentValue(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, iccManager.STK_CMD_TIMER_MANAGEMENT);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, iccManager.STK_TIMER_GET_CURRENT_VALUE);
  is(cmd.options.timerAction, iccManager.STK_TIMER_GET_CURRENT_VALUE);
  is(cmd.options.timerId, 0x08);

  runNextTest();
}

let tests = [
  {command: "d009810301260082028182",
   func: testLocalInfoLocation},
  {command: "d009810301260182028182",
   func: testLocalInfoImei},
  {command: "d009810301260382028182",
   func: testLocalInfoDate},
  {command: "d009810301260482028182",
   func: testLocalInfoLanguage},
  {command: "d011810301270082028182a40101a503102030",
   func: testTimerManagementStart},
  {command: "d00c810301270182028182a40104",
   func: testTimerManagementDeactivate},
  {command: "d00c810301270282028182a40108",
   func: testTimerManagementGetCurrentValue},
 ];

runNextTest();
