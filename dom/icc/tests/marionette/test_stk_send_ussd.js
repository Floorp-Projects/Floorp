/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSendUSSD(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SEND_USSD, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  if (command.options.text) {
    is(command.options.text, expect.title, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d050810301120082028183850a372d62697420555353448a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_1",
            commandQualifier: 0x00,
            title: "7-bit USSD"}},
  {command: "d058810301120082028183850a382d62697420555353448a41444142434445464748494a4b4c4d4e4f505152535455565758595a2d6162636465666768696a6b6c6d6e6f707172737475767778797a2d31323334353637383930",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_2",
            commandQualifier: 0x00,
            title: "8-bit USSD"}},
  {command: "d02f81030112008202818385095543533220555353448a1948041704140420041004120421042204120423041904220415",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_3",
            commandQualifier: 0x00,
            title: "UCS2 USSD"}},
  {command: "d081fd8103011200820281838581b66f6e636520612052454c4541534520434f4d504c455445206d65737361676520636f6e7461696e696e672074686520555353442052657475726e20526573756c74206d657373616765206e6f7420636f6e7461696e696e6720616e206572726f7220686173206265656e2072656365697665642066726f6d20746865206e6574776f726b2c20746865204d45207368616c6c20696e666f726d207468652053494d20746861742074686520636f6d6d616e64206861738a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_4",
            commandQualifier: 0x00,
            title: "once a RELEASE COMPLETE message containing the USSD Return Result message not containing an error has been received from the network, the ME shall inform the SIM that the command has"}},
  {command: "d04681030112008202818385008a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_5",
         commandQualifier: 0x00,
         title: ""}},
  {command: "d054810301120082028183850a42617369632049636f6e8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e5609e020001",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_6",
            commandQualifier: 0x00,
            title: "Basic Icon"}},
  {command: "d054810301120082028183850a436f6c6f722049636f6e8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e5609e020002",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_7",
            commandQualifier: 0x00,
            title: "Color Icon"}},
  {command: "d054810301120082028183850a42617369632049636f6e8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e5609e020101",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_8",
            commandQualifier: 0x00,
            title: "Basic Icon"}},
  {command: "d05f8103011200820281838519800417041404200410041204210422041204230419042204158a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_9",
            commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_10",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d0568103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_11",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001001b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_12",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d0568103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_13",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001002b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_14",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d0568103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_15",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001004b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_16",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_17",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_18",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001008b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_19",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_20",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_21",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001010b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_22",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_23",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_24",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001020b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_25",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_26",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_27",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001040b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_28",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_29",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_30",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001080b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_31",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_32",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d0568103011200820281838510546578742041747472696275746520338a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_33",
            commandQualifier: 0x00,
            title: "Text Attribute 3"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_34",
            commandQualifier: 0x00,
            title: "Text Attribute 1"}},
  {command: "d0568103011200820281838510546578742041747472696275746520328a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_35",
            commandQualifier: 0x00,
            title: "Text Attribute 2"}},
  {command: "d04b8103011200820281838505804f60597d8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_36",
            commandQualifier: 0x00,
            title: "你好"}},
  {command: "d04981030112008202818385038030eb8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   func: testSendUSSD,
   expect: {name: "send_ussd_cmd_37",
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
