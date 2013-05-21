/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSetupCall(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SET_UP_CALL, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  if (command.options.confirmMessage) {
    is(command.options.confirmMessage, expect.text, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d01e81030110008202818385084e6f7420627573798609911032042143651c2c",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_1",
            commandQualifier: 0x00,
            text: "Not busy"}},
  {command: "d01d81030110028202818385074f6e20686f6c648609911032042143651c2c",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_2",
            commandQualifier: 0x02,
            text: "On hold"}},
  {command: "d020810301100482028183850a446973636f6e6e6563748609911032042143651c2c",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_3",
            commandQualifier: 0x04,
            text: "Disconnect"}},
  {command: "d02b81030110008202818385114361706162696c69747920636f6e6669678609911032042143651c2c870201a0",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_4",
            commandQualifier: 0x00,
            text: "Capability config"}},
  {command: "d01c81030110018202818386119110325476981032547698103254769810",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_5",
            commandQualifier: 0x01}},
  {command: "d081fd8103011001820281838581ed54687265652074797065732061726520646566696e65643a202d2073657420757020612063616c6c2c20627574206f6e6c79206966206e6f742063757272656e746c792062757379206f6e20616e6f746865722063616c6c3b202d2073657420757020612063616c6c2c2070757474696e6720616c6c206f746865722063616c6c732028696620616e7929206f6e20686f6c643b202d2073657420757020612063616c6c2c20646973636f6e6e656374696e6720616c6c206f746865722063616c6c732028696620616e79292066697273742e20466f722065616368206f662074686573652074797065732c2086029110",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_6",
            commandQualifier: 0x01,
            text: "Three types are defined: - set up a call, but only if not currently busy on another call; - set up a call, putting all other calls (if any) on hold; - set up a call, disconnecting all other calls (if any) first. For each of these types, "}},
  {command: "d02b810301100082028183850c43616c6c65642070617274798609911032042143651c2c880780509595959595",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_7",
            commandQualifier: 0x00,
            text: "Called party"}},
  {command: "d02281030110018202818385084475726174696f6e8609911032042143651c2c8402010a",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_8",
            commandQualifier: 0x01,
            text: "Duration"}},
  {command: "d028810301100082028183850c434f4e4649524d4154494f4e8609911032042143651c2c850443414c4c",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_9",
            commandQualifier: 0x00,
            text: "CONFIRMATION"}},
  {command: "d03081030110008202818385165365742075702063616c6c2049636f6e20332e312e318609911032042143651c2c9e020101",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_10",
            commandQualifier: 0x00,
            text: "Set up call Icon 3.1.1"}},
  {command: "d03081030110008202818385165365742075702063616c6c2049636f6e20332e322e318609911032042143651c2c9e020001",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_11",
            commandQualifier: 0x00,
            text: "Set up call Icon 3.2.1"}},
  {command: "d03081030110008202818385165365742075702063616c6c2049636f6e20332e332e318609911032042143651c2c9e020102",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_12",
            commandQualifier: 0x00,
            text: "Set up call Icon 3.3.1"}},
  {command: "d04c81030110008202818385165365742075702063616c6c2049636f6e20332e342e318609911032042143651c2c9e02000185165365742075702063616c6c2049636f6e20332e342e329e020001",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_13",
            commandQualifier: 0x00,
            text: "Set up call Icon 3.4.1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_14",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_15",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e01b4d004000601b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_16",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_17",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e02b4d004000602b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_18",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_19",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e04b4d004000604b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_20",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_21",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_22",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e08b4d004000608b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_23",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_24",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_25",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e10b4d004000610b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_26",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_27",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_28",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e20b4d004000620b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_29",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_30",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_31",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e40b4d004000640b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_32",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_33",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_34",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e80b4d004000680b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_35",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032d004000e00b4d004000600b4",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_36",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20338609911032042143651c2c850643414c4c2033",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_37",
            commandQualifier: 0x00,
            text: "CONFIRMATION 3"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e00b4d0040006004b",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_38",
            commandQualifier: 0x00,
            text: "CONFIRMATION 1"}},
  {command: "d02c810301100082028183850e434f4e4649524d4154494f4e20328609911032042143651c2c850643414c4c2032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_39",
            commandQualifier: 0x00,
            text: "CONFIRMATION 2"}},
  {command: "d02d810301100082028183851980041704140420041004120421042204120423041904220415860791103204214365",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_40",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d04c810301100082028183851b800417041404200410041204210422041204230419042204150031860791103204214365851b800417041404200410041204210422041204230419042204150032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_41",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ1"}},
  {command: "d0198103011000820281838505804e0d5fd9860791103204214365",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_42",
            commandQualifier: 0x00,
            text: "不忙"}},
  {command: "d022810301100082028183850580786e5b9a860791103204214365850780625375358bdd",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_43",
            commandQualifier: 0x00,
            text: "确定"}},
  {command: "d01781030110008202818385038030eb860791103204214365",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_44",
            commandQualifier: 0x00,
            text: "ル"}},
  {command: "d02081030110008202818385058030eb003186079110320421436585058030eb0032",
   func: testSetupCall,
   expect: {name: "setup_call_cmd_45",
            commandQualifier: 0x00,
            text: "ル1"}},
];

let pendingEmulatorCmdCount = 0;
function sendStkPduToEmulator(command, func, expect) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(command, function (result) {
    --pendingEmulatorCmdCount;
    is(result[0], "OK");
  });

  icc.onstkcommand = function (evt) {
    func(evt.command, expect);
  }
}

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  let command = "stk pdu " + test.command;
  sendStkPduToEmulator(command, test.func, test.expect)
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

runNextTest();
