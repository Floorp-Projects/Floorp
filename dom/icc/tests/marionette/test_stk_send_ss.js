/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSendSS(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SEND_SS, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  if (command.options.text) {
    is(command.options.text, expect.title, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d029810301110082028183850c43616c6c20466f7277617264891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_1",
            commandQualifier: 0x00,
            title: "Call Forward"}},
  {command: "d02d810301110082028183850c43616c6c20466f7277617264891491aa120a21436587092143658709214365a711fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_2",
            commandQualifier: 0x00,
            title: "Call Forward"}},
  {command: "d081fd8103011100820281838581eb4576656e20696620746865204669786564204469616c6c696e67204e756d626572207365727669636520697320656e61626c65642c2074686520737570706c656d656e74617279207365727669636520636f6e74726f6c20737472696e6720696e636c7564656420696e207468652053454e442053532070726f61637469766520636f6d6d616e64207368616c6c206e6f7420626520636865636b656420616761696e73742074686f7365206f66207468652046444e206c6973742e2055706f6e20726563656976696e67207468697320636f6d6d616e642c20746865204d45207368616c6c20646563698904ffba13fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_3",
            commandQualifier: 0x00,
            title: "Even if the Fixed Dialling Number service is enabled, the supplementary service control string included in the SEND SS proactive command shall not be checked against those of the FDN list. Upon receiving this command, the ME shall deci"}},
  {command: "d01d8103011100820281838500891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_4",
            commandQualifier: 0x00,
            title: ""}},
  {command: "d02b810301110082028183850a42617369632049636f6e891091aa120a214365870921436587a901fb9e020001",
   func: testSendSS,
   expect: {name: "send_ss_cmd_5",
            commandQualifier: 0x00,
            title: "Basic Icon"}},
  {command: "d02c810301110082028183850b436f6c6f75722049636f6e891091aa120a214365870921436587a901fb9e020002",
   func: testSendSS,
   expect: {name: "send_ss_cmd_6",
            commandQualifier: 0x00,
            title: "Colour Icon"}},
  {command: "d02b810301110082028183850a42617369632049636f6e891091aa120a214365870921436587a901fb9e020101",
   func: testSendSS,
   expect: {name: "send_ss_cmd_7",
            commandQualifier: 0x00,
            title: "Basic Icon"}},
  {command: "d036810301110082028183851980041704140420041004120421042204120423041904220415891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_8",
            commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_9",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d02d810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_10",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001001b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_11",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d02d810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_12",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001002b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_13",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d02d810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_14",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001004b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_15",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_16",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_17",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001008b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_18",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_19",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_20",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001010b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_21",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_22",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_23",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001020b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_24",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_25",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_26",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001040b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_27",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_28",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_29",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001080b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_30",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d033810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_31",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d02d810301110082028183851054657874204174747269627574652033891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_32",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d033810301110082028183851054657874204174747269627574652031891091aa120a214365870921436587a901fbd004001000b4",
   func: testSendSS,
   expect: {name: "send_ss_cmd_33",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d02d810301110082028183851054657874204174747269627574652032891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_34",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0228103011100820281838505804f60597d891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_35",
            commandQualifier: 0x00,
            title: "你好"}},
  {command: "d02081030111008202818385038030eb891091aa120a214365870921436587a901fb",
   func: testSendSS,
   expect: {name: "send_ss_cmd_36",
            commandQualifier: 0x00,
            title: "ル"}}
];

// TODO - Bug 843455: Import scripts for marionette tests.
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
